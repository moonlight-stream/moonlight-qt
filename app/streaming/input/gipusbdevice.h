#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef void (^GipUsbDataHandler)(NSData* data);
typedef void (^GipUsbDisconnectHandler)(void);

// Finds, opens, and talks to a GIP-protocol USB controller via the
// IOUSBHost framework. No HID class driver exists for these devices on
// macOS, so this claims the raw interrupt IN/OUT endpoints directly - the
// same USB-level access any userspace driver for an unclaimed device would
// use, nothing privileged about it.
@interface GipUsbDevice : NSObject

// Scans currently connected USB devices for one exposing the GIP data
// interface signature (interface class 0xff, subclass 0x47, protocol 0xd0,
// interface number 0) under a vendor ID known to ship Microsoft-licensed
// Xbox accessories. Returns nil if none is found or opening fails - safe to
// call again on a retry timer, including for devices this call itself just
// nudged into a configured state (see the .mm file's fallback path).
+ (nullable instancetype)findAndOpen;

@property(nonatomic, readonly) uint16_t vendorID;
@property(nonatomic, readonly) uint16_t productID;

// Sends a single packet on the interrupt OUT endpoint synchronously.
- (BOOL)sendPacket:(NSData*)data;

// Arms continuous asynchronous reads on the interrupt IN endpoint.
// dataHandler is invoked on an internal serial queue for each completed
// read; disconnectHandler is invoked once (also on that queue) if the
// device is removed or I/O fails unrecoverably. Only one pair of handlers
// may be active at a time.
- (void)startReadingWithDataHandler:(GipUsbDataHandler)dataHandler
                   disconnectHandler:(GipUsbDisconnectHandler)disconnectHandler;

// Aborts pending I/O and tears down the interface. Safe to call more than
// once, and safe to call from any thread.
- (void)close;

@end

NS_ASSUME_NONNULL_END
