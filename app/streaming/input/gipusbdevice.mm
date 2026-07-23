#import "gipusbdevice.h"

#import <IOKit/IOKitLib.h>
#import <IOKit/IOMessage.h>
#import <IOKit/usb/USBSpec.h>
#import <IOUSBHost/IOUSBHost.h>

namespace {

// Vendor IDs of manufacturers known to ship Microsoft-licensed Xbox
// accessories using the GIP protocol, per xone's transport/wired.c device
// table (xone_wired_id_table).
const uint16_t kKnownVendorIDs[] = {
    0x045e, // Microsoft
    0x0738, // Mad Catz
    0x0e6f, // PDP
    0x0f0d, // Hori
    0x1532, // Razer
    0x24c6, // PowerA
    0x20d6, // BDA
    0x044f, // Thrustmaster
    0x10f5, // Turtle Beach
    0x2e24, // Hyperkin
    0x3285, // Nacon
    0x2dc8, // 8BitDo
    0x2e95, // SCUF
    0x3537, // GameSir
    0x11c1, // unidentified in xone's own table
    0x294b, // Snakebyte
    0x2c16, // Prifereential
    0x0b05, // ASUS
    0x413d, // BIGBIG WON
    0x046d, // Logitech Astro
    0x0079, // EasySMX
    0x1038, // SteelSeries
    0x1b1c, // Corsair
};

// GIP's wired USB interface signature, per xone's XONE_WIRED_VENDOR macro:
// vendor-specific class, subclass 0x47, protocol 0xd0, on interface 0 (the
// data interface; audio, when present, is interface 1).
constexpr uint8_t kGipInterfaceClass = 0xff;
constexpr uint8_t kGipInterfaceSubclass = 0x47;
constexpr uint8_t kGipInterfaceProtocol = 0xd0;
constexpr uint8_t kGipDataInterfaceNumber = 0;

constexpr size_t kReadBufferSize = 64;

NSNumber* _Nullable propertyNumber(io_service_t service, NSString* key)
{
    // IORegistryEntryCreateCFProperty follows the CF "create rule" (+1
    // owned). CFBridgingRelease() hands that reference off to Cocoa's normal
    // retain/release as an autoreleased object - unlike a raw __bridge_transfer
    // cast, this works correctly under both ARC and MRC.
    CFTypeRef value = IORegistryEntryCreateCFProperty(service, (__bridge CFStringRef)key, kCFAllocatorDefault, 0);
    return (NSNumber*)CFBridgingRelease(value);
}

// Finds the first child of deviceService matching the GIP data-interface
// signature (interface class 0xff, subclass 0x47, protocol 0xd0, interface
// number 0). Interfaces only exist as children in the registry once the
// device has an active configuration - see the SET_CONFIGURATION handling
// in +openDeviceService:. Returns IO_OBJECT_NULL if none is found; the
// caller is responsible for releasing a non-null result.
//
// This walks the registry directly rather than using
// -[IOUSBHostInterface createMatchingDictionaryWithVendorID:...] because
// that matching dictionary reliably returns zero results for these
// devices in practice, for reasons that don't appear to be documented -
// possibly because it expects to match already-registered
// IOUSBHostInterface services in a way that interacts poorly with devices
// no system driver has ever bound to. The registry walk below was verified
// directly against real hardware.
io_service_t findGipInterfaceChild(io_service_t deviceService)
{
    io_iterator_t iterator = IO_OBJECT_NULL;
    if (IORegistryEntryGetChildIterator(deviceService, kIOServicePlane, &iterator) != KERN_SUCCESS) {
        return IO_OBJECT_NULL;
    }

    io_service_t result = IO_OBJECT_NULL;
    io_service_t child;
    while ((child = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        if (result == IO_OBJECT_NULL) {
            NSNumber* ifaceClass = propertyNumber(child, @"bInterfaceClass");
            NSNumber* ifaceSubclass = propertyNumber(child, @"bInterfaceSubClass");
            NSNumber* ifaceProtocol = propertyNumber(child, @"bInterfaceProtocol");
            NSNumber* ifaceNumber = propertyNumber(child, @"bInterfaceNumber");
            if (ifaceClass.unsignedCharValue == kGipInterfaceClass &&
                ifaceSubclass.unsignedCharValue == kGipInterfaceSubclass &&
                ifaceProtocol.unsignedCharValue == kGipInterfaceProtocol &&
                ifaceNumber.unsignedCharValue == kGipDataInterfaceNumber) {
                result = child;
                continue; // Keep iterating so every child still gets released below.
            }
        }
        IOObjectRelease(child);
    }
    IOObjectRelease(iterator);
    return result;
}

} // namespace

@implementation GipUsbDevice {
    IOUSBHostDevice* _device;
    IOUSBHostInterface* _interface;
    IOUSBHostPipe* _readPipe;
    IOUSBHostPipe* _writePipe;
    dispatch_queue_t _queue;
    GipUsbDataHandler _dataHandler;
    GipUsbDisconnectHandler _disconnectHandler;
    BOOL _closed;
}

+ (nullable instancetype)findAndOpen
{
    // Matching against the generic "IOUSBDevice" class (rather than
    // IOUSBHostDevice's own matching dictionary helper, which doesn't
    // reliably find these devices - see findGipInterfaceChild's comment)
    // mirrors how every other USB enumeration tool on macOS finds raw,
    // driver-less devices.
    CFMutableDictionaryRef matching = IOServiceMatching("IOUSBDevice");
    io_iterator_t iterator = IO_OBJECT_NULL;
    if (IOServiceGetMatchingServices(kIOMainPortDefault, matching, &iterator) != KERN_SUCCESS) {
        return nil;
    }

    GipUsbDevice* result = nil;
    io_service_t service;
    while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        if (result == nil) {
            NSNumber* vendorID = propertyNumber(service, @"idVendor");
            bool isKnownVendor = false;
            for (uint16_t candidate : kKnownVendorIDs) {
                if (vendorID != nil && vendorID.unsignedShortValue == candidate) {
                    isKnownVendor = true;
                    break;
                }
            }
            if (isKnownVendor) {
                result = [self openDeviceService:service];
            }
        }
        IOObjectRelease(service);
    }
    IOObjectRelease(iterator);
    return result;
}

// Opens deviceService, applies SET_CONFIGURATION if it hasn't been already
// (no system driver claims these devices, so macOS doesn't always do this
// on its own), then finds and opens its GIP data interface.
+ (nullable instancetype)openDeviceService:(io_service_t)deviceService
{
    // device is owned (+1) by this method the whole way through - either
    // consumed by -initWithDevice:interfaceService: (which retains its own
    // reference) or released directly below on every failure path.
    NSError* error = nil;
    IOUSBHostDevice* device = [[IOUSBHostDevice alloc] initWithIOService:deviceService
                                                                   options:IOUSBHostObjectInitOptionsNone
                                                                     queue:nil
                                                                     error:&error
                                                           interestHandler:nil];
    if (device == nil) {
        return nil;
    }

    if (device.configurationDescriptor == NULL) {
        NSError* configError = nil;
        const IOUSBConfigurationDescriptor* firstConfig =
            [device configurationDescriptorWithIndex:0 error:&configError];
        if (firstConfig == NULL ||
            ![device configureWithValue:firstConfig->bConfigurationValue matchInterfaces:YES error:&configError]) {
            NSLog(@"GipUsbDevice: failed to configure device: %@", configError);
            [device destroy];
            [device release];
            return nil;
        }
    }

    io_service_t interfaceService = findGipInterfaceChild(deviceService);
    if (interfaceService == IO_OBJECT_NULL) {
        [device destroy];
        [device release];
        return nil;
    }

    // -initWithDevice:interfaceService: retains its own reference to device,
    // and on failure its -dealloc (via -close) already calls [device destroy]
    // - so don't do that again here, just drop our local +1 reference either way.
    GipUsbDevice* result = [[GipUsbDevice alloc] initWithDevice:device interfaceService:interfaceService];
    IOObjectRelease(interfaceService);
    [device release];
    return [result autorelease];
}

- (nullable instancetype)initWithDevice:(IOUSBHostDevice*)device interfaceService:(io_service_t)service
{
    self = [super init];
    if (self == nil) {
        return nil;
    }

    _device = [device retain];
    _queue = dispatch_queue_create("com.moonlight-stream.gipbridge.usb", DISPATCH_QUEUE_SERIAL);

    __weak GipUsbDevice* weakSelf = self;
    NSError* error = nil;
    _interface = [[IOUSBHostInterface alloc]
        initWithIOService:service
                   options:IOUSBHostObjectInitOptionsNone
                     queue:_queue
                     error:&error
           interestHandler:^(IOUSBHostObject* hostObject, uint32_t messageType, void* messageArgument) {
               (void)hostObject;
               (void)messageArgument;
               if (messageType == kIOMessageServiceIsTerminated) {
                   [weakSelf handleDisconnect];
               }
           }];
    if (_interface == nil) {
        NSLog(@"GipUsbDevice: failed to open interface: %@", error);
        // -dealloc (triggered by -release below) calls -close, which tears
        // down whatever of _device/_interface/_queue got set above -
        // messaging nil is always safe for whichever ones didn't.
        [self release];
        return nil;
    }

    if (![self resolveEndpointsAndOpenPipes]) {
        [self release];
        return nil;
    }

    const IOUSBDeviceDescriptor* deviceDesc = _interface.deviceDescriptor;
    _vendorID = deviceDesc != NULL ? deviceDesc->idVendor : 0;
    _productID = deviceDesc != NULL ? deviceDesc->idProduct : 0;

    return self;
}

- (BOOL)resolveEndpointsAndOpenPipes
{
    const IOUSBInterfaceDescriptor* ifaceDesc = _interface.interfaceDescriptor;
    const IOUSBConfigurationDescriptor* configDesc = _interface.configurationDescriptor;
    if (ifaceDesc == NULL || configDesc == NULL) {
        NSLog(@"GipUsbDevice: missing interface/configuration descriptor");
        return NO;
    }

    // Walk the raw descriptor bytes following the interface descriptor to
    // find its endpoint descriptors, stopping at the next interface (or the
    // end of the configuration). IOUSBHost's userspace API doesn't expose
    // the StandardUSB descriptor-walking helpers - those are gated to
    // kernel/DriverKit builds (`#if defined(__cplusplus) && KERNEL` in
    // StandardUSB.h) - so this does the same walk by hand.
    const uint8_t* configBytes = reinterpret_cast<const uint8_t*>(configDesc);
    const uint8_t* end = configBytes + configDesc->wTotalLength;
    const uint8_t* cursor = reinterpret_cast<const uint8_t*>(ifaceDesc) + ifaceDesc->bLength;

    NSNumber* inAddress = nil;
    NSNumber* outAddress = nil;

    while (cursor + 2 <= end) {
        uint8_t length = cursor[0];
        uint8_t type = cursor[1];
        if (length < 2 || cursor + length > end) {
            break;
        }
        if (type == kUSBInterfaceDesc) {
            // Reached the next interface; no more endpoints for this one.
            break;
        }
        if (type == kUSBEndpointDesc && length >= sizeof(IOUSBEndpointDescriptor)) {
            const auto* ep = reinterpret_cast<const IOUSBEndpointDescriptor*>(cursor);
            uint8_t transferType = ep->bmAttributes & 0x03;
            if (transferType == kUSBInterrupt) {
                if (ep->bEndpointAddress & 0x80) {
                    inAddress = @(ep->bEndpointAddress);
                } else {
                    outAddress = @(ep->bEndpointAddress);
                }
            }
        }
        cursor += length;
    }

    if (inAddress == nil || outAddress == nil) {
        NSLog(@"GipUsbDevice: no interrupt IN/OUT endpoint pair found");
        return NO;
    }

    NSError* error = nil;
    _readPipe = [_interface copyPipeWithAddress:inAddress.unsignedIntegerValue error:&error];
    if (_readPipe == nil) {
        NSLog(@"GipUsbDevice: failed to open read pipe: %@", error);
        return NO;
    }

    _writePipe = [_interface copyPipeWithAddress:outAddress.unsignedIntegerValue error:&error];
    if (_writePipe == nil) {
        NSLog(@"GipUsbDevice: failed to open write pipe: %@", error);
        return NO;
    }

    return YES;
}

- (BOOL)sendPacket:(NSData*)data
{
    if (_closed) {
        return NO;
    }

    NSError* error = nil;
    NSMutableData* buffer = [_interface ioDataWithCapacity:data.length error:&error];
    if (buffer == nil) {
        NSLog(@"GipUsbDevice: failed to allocate write buffer: %@", error);
        return NO;
    }
    [buffer replaceBytesInRange:NSMakeRange(0, data.length) withBytes:data.bytes];

    // completionTimeout must be 0 (never-timeout) for interrupt pipes, per
    // -[IOUSBHostPipe sendIORequestWithData:...]'s docs.
    NSUInteger bytesTransferred = 0;
    BOOL ok = [_writePipe sendIORequestWithData:buffer
                               bytesTransferred:&bytesTransferred
                              completionTimeout:0
                                           error:&error];
    if (!ok) {
        NSLog(@"GipUsbDevice: write failed: %@", error);
    }
    return ok;
}

- (void)startReadingWithDataHandler:(GipUsbDataHandler)dataHandler
                   disconnectHandler:(GipUsbDisconnectHandler)disconnectHandler
{
    _dataHandler = [dataHandler copy];
    _disconnectHandler = [disconnectHandler copy];
    [self armRead];
}

- (void)armRead
{
    if (_closed) {
        return;
    }

    NSError* error = nil;
    NSMutableData* buffer = [_interface ioDataWithCapacity:kReadBufferSize error:&error];
    if (buffer == nil) {
        NSLog(@"GipUsbDevice: failed to allocate read buffer: %@", error);
        [self handleDisconnect];
        return;
    }

    __weak GipUsbDevice* weakSelf = self;
    BOOL ok = [_readPipe enqueueIORequestWithData:buffer
                                 completionTimeout:0
                                             error:&error
                                 completionHandler:^(IOReturn status, NSUInteger bytesTransferred) {
                                     [weakSelf handleReadCompletion:status
                                                                data:buffer
                                                    bytesTransferred:bytesTransferred];
                                 }];
    if (!ok) {
        NSLog(@"GipUsbDevice: failed to enqueue read: %@", error);
        [self handleDisconnect];
    }
}

- (void)handleReadCompletion:(IOReturn)status data:(NSData*)buffer bytesTransferred:(NSUInteger)bytesTransferred
{
    if (_closed) {
        return;
    }

    if (status != kIOReturnSuccess && status != kIOReturnAborted) {
        NSLog(@"GipUsbDevice: read failed: 0x%08x", status);
        [self handleDisconnect];
        return;
    }

    if (status == kIOReturnAborted) {
        // Expected during -close; don't re-arm.
        return;
    }

    if (bytesTransferred > 0 && _dataHandler != nil) {
        _dataHandler([buffer subdataWithRange:NSMakeRange(0, bytesTransferred)]);
    }

    [self armRead];
}

- (void)handleDisconnect
{
    if (_closed) {
        return;
    }
    _closed = YES;
    if (_disconnectHandler != nil) {
        _disconnectHandler();
    }
}

- (void)close
{
    // Setting _closed is synchronized through _queue - the same serial
    // queue -handleReadCompletion:data:bytesTransferred: runs on - rather
    // than assigning it directly, so that method is guaranteed to observe
    // it (GCD serial queues run one block at a time, in order: anything
    // already queued there runs before this, anything after sees _closed
    // already true). That's what guarantees no data/disconnect handler
    // fires after -close returns; callers (GipBridgeController's
    // destructor) rely on that to tear down without a dangling callback
    // landing on a freed object. The actual abort is issued asynchronously
    // and outside that block on purpose: waiting for it synchronously from
    // inside a block already running on _queue would deadlock, since the
    // completion it's waiting for is itself serviced on _queue.
    //
    // The block below captures a plain C pointer to _closed rather than
    // self, since -close is also called from -dealloc: capturing self in a
    // block retains it, and briefly resurrecting self mid-dealloc is
    // undefined behavior.
    BOOL* closedFlag = &_closed;
    __block BOOL wasAlreadyClosed = NO;
    dispatch_sync(_queue, ^{
        wasAlreadyClosed = *closedFlag;
        *closedFlag = YES;
    });
    if (wasAlreadyClosed) {
        return;
    }

    NSError* error = nil;
    [_readPipe abortWithOption:IOUSBHostAbortOptionAsynchronous error:&error];
    [_writePipe abortWithOption:IOUSBHostAbortOptionAsynchronous error:&error];
    [_interface destroy];
    [_device destroy];
}

- (void)dealloc
{
    [self close];

    [_dataHandler release];
    [_disconnectHandler release];
    [_readPipe release];
    [_writePipe release];
    [_interface release];
    [_device release];
    if (_queue != nullptr) {
        dispatch_release(_queue);
    }

    [super dealloc];
}

@end
