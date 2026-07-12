#include "gipbridge.h"

namespace {

constexpr int k_RetryIntervalMs = 1000;
constexpr int k_NumAxes = SDL_CONTROLLER_AXIS_MAX;
constexpr int k_NumButtons = SDL_CONTROLLER_BUTTON_DPAD_RIGHT + 1;

// gip_bridge reports triggers as 0..1023; SDL_JoystickSetVirtualAxis()
// expects the full Sint16 range, with SdlInputHandler::handleControllerAxisEvent()
// later rescaling from 0..32767 down to the 0..255 range Moonlight sends
// over the wire.
Sint16 scaleTrigger(uint16_t rawValue)
{
    return static_cast<Sint16>((static_cast<int>(rawValue) * 32767) / 1023);
}

} // namespace

GipBridgeController::GipBridgeController()
    : m_Stop(false),
      m_Device(nullptr),
      m_JoystickDeviceIndex(-1),
      m_Joystick(nullptr)
{
    m_Thread = std::thread(&GipBridgeController::run, this);
}

GipBridgeController::~GipBridgeController()
{
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Stop = true;
    }
    m_Cv.notify_all();

    if (m_Thread.joinable()) {
        m_Thread.join();
    }
}

bool GipBridgeController::waitUnlessStopping(int ms)
{
    std::unique_lock<std::mutex> lock(m_Mutex);
    return !m_Cv.wait_for(lock, std::chrono::milliseconds(ms), [this] { return m_Stop.load(); });
}

int SDLCALL GipBridgeController::rumbleCallback(void* userdata, Uint16 lowFreqRumble, Uint16 highFreqRumble)
{
    auto* self = static_cast<GipBridgeController*>(userdata);
    if (self->m_Device == nullptr) {
        return -1;
    }

    uint8_t left = static_cast<uint8_t>(lowFreqRumble >> 8);
    uint8_t right = static_cast<uint8_t>(highFreqRumble >> 8);
    return gip_send_rumble(self->m_Device, left, right) ? 0 : -1;
}

void GipBridgeController::run()
{
    while (!m_Stop.load()) {
        m_Device = gip_open_matching_device();
        if (m_Device == nullptr) {
            waitUnlessStopping(k_RetryIntervalMs);
            continue;
        }

        SDL_VirtualJoystickDesc desc;
        SDL_zero(desc);
        desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
        desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
        desc.naxes = k_NumAxes;
        desc.nbuttons = k_NumButtons;
        desc.vendor_id = gip_device_vendor_id(m_Device);
        desc.product_id = gip_device_product_id(m_Device);
        desc.name = "GIP Controller";
        desc.userdata = this;
        desc.Rumble = rumbleCallback;

        m_JoystickDeviceIndex = SDL_JoystickAttachVirtualEx(&desc);
        if (m_JoystickDeviceIndex < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "GipBridge: SDL_JoystickAttachVirtualEx() failed: %s",
                         SDL_GetError());
            gip_close(m_Device);
            m_Device = nullptr;
            waitUnlessStopping(k_RetryIntervalMs);
            continue;
        }

        m_Joystick = SDL_JoystickOpen(m_JoystickDeviceIndex);

        GipState state;
        while (!m_Stop.load()) {
            // Bounded by gip_poll_state()'s internal read timeout, so this
            // loop keeps rechecking m_Stop instead of blocking indefinitely
            // if the controller falls silent - see GipPollResult's docs.
            GipPollResult result = gip_poll_state(m_Device, &state);
            if (result == GipPollResult_Error) {
                break;
            }
            if (result == GipPollResult_Timeout) {
                continue;
            }

            SDL_JoystickSetVirtualAxis(m_Joystick, SDL_CONTROLLER_AXIS_LEFTX, state.stick_left_x);
            SDL_JoystickSetVirtualAxis(m_Joystick, SDL_CONTROLLER_AXIS_LEFTY, state.stick_left_y);
            SDL_JoystickSetVirtualAxis(m_Joystick, SDL_CONTROLLER_AXIS_RIGHTX, state.stick_right_x);
            SDL_JoystickSetVirtualAxis(m_Joystick, SDL_CONTROLLER_AXIS_RIGHTY, state.stick_right_y);
            SDL_JoystickSetVirtualAxis(m_Joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, scaleTrigger(state.trigger_left));
            SDL_JoystickSetVirtualAxis(m_Joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, scaleTrigger(state.trigger_right));

            SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_A, state.buttons.a);
            SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_B, state.buttons.b);
            SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_X, state.buttons.x);
            SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_Y, state.buttons.y);
            SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_BACK, state.buttons.back);
            SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_START, state.buttons.start);
            SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_GUIDE, state.buttons.guide);
            SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_LEFTSTICK, state.buttons.left_stick);
            SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_RIGHTSTICK, state.buttons.right_stick);
            SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, state.buttons.left_shoulder);
            SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, state.buttons.right_shoulder);
            SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_DPAD_UP, state.buttons.dpad_up);
            SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_DPAD_DOWN, state.buttons.dpad_down);
            SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_DPAD_LEFT, state.buttons.dpad_left);
            SDL_JoystickSetVirtualButton(m_Joystick, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, state.buttons.dpad_right);
        }

        // The device disconnected (or we're shutting down). Tear down and,
        // unless stopping, resume scanning for a reconnect.
        if (m_Joystick != nullptr) {
            SDL_JoystickClose(m_Joystick);
            m_Joystick = nullptr;
        }
        SDL_JoystickDetachVirtual(m_JoystickDeviceIndex);
        m_JoystickDeviceIndex = -1;

        gip_close(m_Device);
        m_Device = nullptr;
    }
}
