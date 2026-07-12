#include "gipbridge.h"
#include "gipprotocol.h"

#import "gipusbdevice.h"

namespace {

constexpr int64_t kScanIntervalNs = 1LL * NSEC_PER_SEC;

// Matches the duration_ms SdlInputHandler::rumble() passes to
// SDL_GameControllerRumble() - see m_RumbleWatchdogDeadline's comment.
constexpr double kRumbleWatchdogSeconds = 30.0;
constexpr int kNumAxes = SDL_CONTROLLER_AXIS_MAX;
constexpr int kNumButtons = SDL_CONTROLLER_BUTTON_DPAD_RIGHT + 1;

// gip reports triggers as 0..1023; SDL_JoystickSetVirtualAxis() expects the
// full Sint16 range, with SdlInputHandler::handleControllerAxisEvent()
// later rescaling from 0..32767 down to the 0..255 range Moonlight sends
// over the wire.
Sint16 scaleTrigger(uint16_t rawValue)
{
    return static_cast<Sint16>((static_cast<int>(rawValue) * 32767) / 1023);
}

NSData* toNSData(const std::vector<uint8_t>& bytes)
{
    return [NSData dataWithBytes:bytes.data() length:bytes.size()];
}

} // namespace

GipBridgeController::GipBridgeController()
    : m_UsbDevice(nil)
    , m_ScanQueue(dispatch_queue_create("com.moonlight-stream.gipbridge.scan", DISPATCH_QUEUE_SERIAL))
    , m_ScanTimer(nullptr)
    , m_JoystickDeviceIndex(-1)
    , m_Joystick(nullptr)
    , m_GuidePressed(false)
    , m_RumbleSequence(4) // 1 and 3 are used by the power-on/LED-on packets sent in tryOpen()
    , m_RumbleWatchdogDeadline(0)
{
    startScanTimer();
}

GipBridgeController::~GipBridgeController()
{
    dispatch_sync(m_ScanQueue, ^{
        if (m_ScanTimer != nullptr) {
            dispatch_source_cancel(m_ScanTimer);
            dispatch_release(m_ScanTimer);
            m_ScanTimer = nullptr;
        }
        [m_UsbDevice close];
    });

    // Second sync call, deliberately separate from the one above: GipUsbDevice's
    // read-completion callback runs on its own queue, and in a narrow window it
    // can observe "not closed yet" and dispatch one more handleData/handleDisconnect
    // block onto m_ScanQueue concurrently with the close() call above. Since
    // m_ScanQueue is serial (FIFO), this second sync is guaranteed not to return
    // until any such straggler has already run - only then is it safe to finish
    // tearing down the state those callbacks touch.
    dispatch_sync(m_ScanQueue, ^{
        detachVirtualJoystick();
        [m_UsbDevice release];
        m_UsbDevice = nil;
    });

    dispatch_release(m_ScanQueue);
}

void GipBridgeController::startScanTimer()
{
    m_ScanTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, m_ScanQueue);
    dispatch_source_set_timer(m_ScanTimer, dispatch_time(DISPATCH_TIME_NOW, 0), kScanIntervalNs, kScanIntervalNs / 10);
    dispatch_source_set_event_handler(m_ScanTimer, ^{
        tryOpen();
        checkRumbleWatchdog();
    });
    dispatch_resume(m_ScanTimer);
}

// Runs on m_ScanQueue (called directly from the scan timer's event handler).
void GipBridgeController::tryOpen()
{
    if (m_UsbDevice != nil) {
        return;
    }

    GipUsbDevice* device = [GipUsbDevice findAndOpen];
    if (device == nil) {
        return;
    }

    // +findAndOpen returns an autoreleased reference; m_UsbDevice is a plain
    // C++ member (this file compiles under MRC, matching the rest of the
    // project's .mm files), so it needs its own explicit retain.
    m_UsbDevice = [device retain];
    m_PendingBytes.clear();
    m_GuidePressed = false;
    m_RumbleSequence = 4;
    m_RumbleWatchdogDeadline = 0;

    // Without the power-on packet, wired GIP controllers (Xbox One/Series-
    // generation, which is what these third-party pads emulate) stay
    // completely silent - the interface claims fine and the OS enumerates
    // it normally, but no input reports ever arrive. Without the LED
    // packet, the controller stays in its firmware-default blinking
    // "searching for connection" pattern indefinitely, even once input is
    // flowing.
    //
    // Known limitation: there's no equivalent "go back to blinking" packet
    // sent on disconnect, so the LED stays solid after the app quits even
    // though the wired connection is still up. Tried sending the documented
    // GIP LED blink/fade modes (xone's enum gip_led_mode: BLINK_FAST/
    // NORMAL/SLOW = 0x02/0x03/0x04, FADE_SLOW/FAST = 0x08/0x09) on a real
    // 8BitDo pad and none of them visibly changed anything - only OFF
    // (0x00) and ON (0x01, used below) had any visible effect on this
    // controller's firmware. Purely cosmetic, deliberately not chasing it
    // further for now.
    [m_UsbDevice sendPacket:toNSData(GipProtocol::buildPowerOnPacket())];
    [m_UsbDevice sendPacket:toNSData(GipProtocol::buildLedOnPacket(GipProtocol::LED_BRIGHTNESS_DEFAULT))];

    attachVirtualJoystick();

    GipBridgeController* controller = this;
    dispatch_queue_t scanQueue = m_ScanQueue;
    [m_UsbDevice
        startReadingWithDataHandler:^(NSData* data) {
            dispatch_async(scanQueue, ^{
                controller->handleData(static_cast<const uint8_t*>(data.bytes), data.length);
            });
        }
        disconnectHandler:^{
            dispatch_async(scanQueue, ^{
                controller->handleDisconnect();
            });
        }];
}

// Runs on m_ScanQueue.
void GipBridgeController::handleData(const uint8_t* bytes, size_t length)
{
    m_PendingBytes.insert(m_PendingBytes.end(), bytes, bytes + length);

    size_t consumed = 0;
    for (;;) {
        GipProtocol::Header header;
        size_t payloadOffset;
        if (!GipProtocol::decodeHeader(m_PendingBytes.data() + consumed, m_PendingBytes.size() - consumed, header,
                                        payloadOffset)) {
            break;
        }
        size_t packetEnd = payloadOffset + header.packetLength;
        if (packetEnd > m_PendingBytes.size() - consumed) {
            break;
        }
        const uint8_t* payload = m_PendingBytes.data() + consumed + payloadOffset;

        if (header.options & GipProtocol::OPT_ACKNOWLEDGE) {
            [m_UsbDevice
                sendPacket:toNSData(GipProtocol::buildAckPacket(header.command, header.sequence,
                                                                 static_cast<uint16_t>(header.packetLength)))];
        }

        if (header.command == GipProtocol::CMD_VIRTUAL_KEY) {
            bool guidePressed = false;
            if (GipProtocol::parseVirtualKey(payload, header.packetLength, guidePressed)) {
                m_GuidePressed = guidePressed;
            }
        } else if (header.command == GipProtocol::CMD_INPUT && m_Joystick != nullptr) {
            GipProtocol::InputState state;
            if (GipProtocol::parseInputReport(payload, header.packetLength, state)) {
                state.guide = m_GuidePressed;

                SDL_JoystickSetVirtualAxis(m_Joystick, SDL_CONTROLLER_AXIS_LEFTX, state.stickLeftX);
                SDL_JoystickSetVirtualAxis(m_Joystick, SDL_CONTROLLER_AXIS_LEFTY, state.stickLeftY);
                SDL_JoystickSetVirtualAxis(m_Joystick, SDL_CONTROLLER_AXIS_RIGHTX, state.stickRightX);
                SDL_JoystickSetVirtualAxis(m_Joystick, SDL_CONTROLLER_AXIS_RIGHTY, state.stickRightY);
                SDL_JoystickSetVirtualAxis(m_Joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, scaleTrigger(state.triggerLeft));
                SDL_JoystickSetVirtualAxis(m_Joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT,
                                            scaleTrigger(state.triggerRight));

                SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_A, state.a);
                SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_B, state.b);
                SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_X, state.x);
                SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_Y, state.y);
                SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_BACK, state.view);
                SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_START, state.menu);
                SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_GUIDE, state.guide);
                SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_LEFTSTICK, state.stickLeftClick);
                SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_RIGHTSTICK, state.stickRightClick);
                SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, state.bumperLeft);
                SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, state.bumperRight);
                SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_DPAD_UP, state.dpadUp);
                SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_DPAD_DOWN, state.dpadDown);
                SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_DPAD_LEFT, state.dpadLeft);
                SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, state.dpadRight);
            }
        }

        consumed += packetEnd;
    }

    if (consumed > 0) {
        m_PendingBytes.erase(m_PendingBytes.begin(), m_PendingBytes.begin() + static_cast<ptrdiff_t>(consumed));
    }
}

// Runs on m_ScanQueue. The scan timer keeps running the whole time (it no-ops
// while m_UsbDevice is set - see tryOpen), so a reconnect is picked up
// automatically without any extra bookkeeping here.
void GipBridgeController::handleDisconnect()
{
    detachVirtualJoystick();
    [m_UsbDevice close];
    [m_UsbDevice release];
    m_UsbDevice = nil;
}

void GipBridgeController::attachVirtualJoystick()
{
    SDL_VirtualJoystickDesc desc;
    SDL_zero(desc);
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    desc.naxes = kNumAxes;
    desc.nbuttons = kNumButtons;
    desc.vendor_id = m_UsbDevice.vendorID;
    desc.product_id = m_UsbDevice.productID;
    desc.name = "GIP Controller";
    desc.userdata = this;
    desc.Rumble = rumbleCallback;

    m_JoystickDeviceIndex = SDL_JoystickAttachVirtualEx(&desc);
    if (m_JoystickDeviceIndex < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "GipBridge: SDL_JoystickAttachVirtualEx() failed: %s",
                     SDL_GetError());
        return;
    }

    m_Joystick = SDL_JoystickOpen(m_JoystickDeviceIndex);
}

void GipBridgeController::detachVirtualJoystick()
{
    if (m_Joystick != nullptr) {
        SDL_JoystickClose(m_Joystick);
        m_Joystick = nullptr;
    }
    if (m_JoystickDeviceIndex >= 0) {
        SDL_JoystickDetachVirtual(m_JoystickDeviceIndex);
        m_JoystickDeviceIndex = -1;
    }
}

int SDLCALL GipBridgeController::rumbleCallback(void* userdata, Uint16 lowFreqRumble, Uint16 highFreqRumble)
{
    auto* self = static_cast<GipBridgeController*>(userdata);
    if (self->m_UsbDevice == nil) {
        return -1;
    }

    uint8_t left = static_cast<uint8_t>(lowFreqRumble >> 8);
    uint8_t right = static_cast<uint8_t>(highFreqRumble >> 8);

    uint8_t sequence = self->m_RumbleSequence++;
    if (self->m_RumbleSequence == 0) {
        self->m_RumbleSequence = 1; // 0 is reserved/invalid, per GIP's sequence numbering.
    }

    // Arm/disarm the watchdog (see m_RumbleWatchdogDeadline's comment) - a
    // non-zero command starts/refreshes the 30s deadline, an explicit stop
    // (0,0) - whether from the host or the watchdog itself - disarms it.
    self->m_RumbleWatchdogDeadline =
        (left == 0 && right == 0) ? 0 : CFAbsoluteTimeGetCurrent() + kRumbleWatchdogSeconds;

    BOOL ok = [self->m_UsbDevice sendPacket:toNSData(GipProtocol::buildRumblePacket(left, right, sequence))];
    return ok ? 0 : -1;
}

// Runs on m_ScanQueue (called from the scan timer's event handler, so this
// gets checked once a second whenever a device is connected).
void GipBridgeController::checkRumbleWatchdog()
{
    if (m_UsbDevice == nil || m_RumbleWatchdogDeadline == 0) {
        return;
    }
    if (CFAbsoluteTimeGetCurrent() < m_RumbleWatchdogDeadline) {
        return;
    }

    m_RumbleWatchdogDeadline = 0;
    NSLog(@"GipBridge: rumble watchdog fired - no update in %.0fs, forcing motors off", kRumbleWatchdogSeconds);

    uint8_t sequence = m_RumbleSequence++;
    if (m_RumbleSequence == 0) {
        m_RumbleSequence = 1;
    }
    [m_UsbDevice sendPacket:toNSData(GipProtocol::buildRumblePacket(0, 0, sequence))];
}
