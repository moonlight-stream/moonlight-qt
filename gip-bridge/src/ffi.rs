//! C ABI for the GIP controller bridge: open a device, poll parsed input
//! state, send rumble, and close it - for drivers written outside Rust.

use crate::gip;
use crate::usb;
use std::io::Read;
use std::ptr;
use std::time::Duration;

/// How long `gip_poll_state` blocks waiting for a report before returning
/// `GipPollResult::Timeout`. Bounds how long a caller's shutdown sequence
/// can stall inside a blocking poll call - short enough to stay responsive,
/// long enough not to busy-loop.
const READ_TIMEOUT: Duration = Duration::from_millis(250);

/// Outcome of [`gip_poll_state`].
#[repr(C)]
#[derive(Clone, Copy, PartialEq, Eq)]
pub enum GipPollResult {
    /// A new report was parsed and written to `out`.
    State = 0,
    /// No report arrived within the timeout; `out` was not touched. Not an
    /// error - callers should just poll again.
    Timeout = 1,
    /// The device disconnected or a USB error occurred; `out` was not
    /// touched. The caller should `gip_close` and stop polling.
    Error = 2,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct GipButtons {
    pub a: bool,
    pub b: bool,
    pub x: bool,
    pub y: bool,
    pub back: bool,
    pub start: bool,
    pub left_stick: bool,
    pub right_stick: bool,
    pub left_shoulder: bool,
    pub right_shoulder: bool,
    pub dpad_up: bool,
    pub dpad_down: bool,
    pub dpad_left: bool,
    pub dpad_right: bool,
    pub guide: bool,
}

#[repr(C)]
#[derive(Clone, Copy, Default)]
pub struct GipState {
    pub buttons: GipButtons,
    pub trigger_left: u16,
    pub trigger_right: u16,
    pub stick_left_x: i16,
    pub stick_left_y: i16,
    pub stick_right_x: i16,
    pub stick_right_y: i16,
}

impl From<gip::input::GamepadState> for GipState {
    fn from(s: gip::input::GamepadState) -> Self {
        GipState {
            buttons: GipButtons {
                a: s.a,
                b: s.b,
                x: s.x,
                y: s.y,
                back: s.view,
                start: s.menu,
                left_stick: s.stick_left_click,
                right_stick: s.stick_right_click,
                left_shoulder: s.bumper_left,
                right_shoulder: s.bumper_right,
                dpad_up: s.dpad_up,
                dpad_down: s.dpad_down,
                dpad_left: s.dpad_left,
                dpad_right: s.dpad_right,
                // Reported via a separate GIP_CMD_VIRTUAL_KEY packet, not
                // part of the input report this conversion is built from;
                // gip_poll_state() fills it in afterward.
                guide: false,
            },
            trigger_left: s.trigger_left,
            trigger_right: s.trigger_right,
            stick_left_x: s.stick_left_x,
            stick_left_y: s.stick_left_y,
            stick_right_x: s.stick_right_x,
            stick_right_y: s.stick_right_y,
        }
    }
}

/// Opaque handle for an open, handshaken GIP controller.
pub struct GipDevice {
    usb: usb::GipUsb,
    reader: nusb::io::EndpointRead<nusb::transfer::Interrupt>,
    writer: nusb::io::EndpointWrite<nusb::transfer::Interrupt>,
    pending: Vec<u8>,
    buf: [u8; 64],
    /// Guide/Xbox button state, last reported via a `GIP_CMD_VIRTUAL_KEY`
    /// packet. Carried forward and merged into each `GipState` returned by
    /// `gip_poll_state`, since it doesn't arrive as part of the input report.
    guide_pressed: bool,
}

/// Finds and opens the first matching GIP controller currently connected and
/// completes the power-on handshake. Returns NULL if none is connected or
/// setup fails - safe to call again on a retry/hotplug timer.
#[unsafe(no_mangle)]
pub extern "C" fn gip_open_matching_device() -> *mut GipDevice {
    let result = (|| -> anyhow::Result<GipDevice> {
        let usb = usb::GipUsb::open()?;
        gip::handshake::power_on(usb.writer(64))?;
        // Without this, the controller stays in its firmware-default
        // blinking "searching for connection" LED pattern indefinitely,
        // even though it's fully connected and streaming input.
        gip::led::led_on(usb.writer(64), gip::LED_BRIGHTNESS_DEFAULT)?;
        let reader = usb.reader(64).with_read_timeout(READ_TIMEOUT);
        let writer = usb.writer(64);
        Ok(GipDevice {
            usb,
            reader,
            writer,
            pending: Vec::new(),
            buf: [0u8; 64],
            guide_pressed: false,
        })
    })();

    match result {
        Ok(dev) => Box::into_raw(Box::new(dev)),
        Err(e) => {
            log::warn!("gip_open_matching_device: {e:#}");
            ptr::null_mut()
        }
    }
}

/// Blocks for up to `READ_TIMEOUT` waiting for the next `GIP_CMD_INPUT`
/// report. See [`GipPollResult`] for what each outcome means. `device` and
/// `out` must be non-null and valid for the duration of the call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gip_poll_state(device: *mut GipDevice, out: *mut GipState) -> GipPollResult {
    let device = unsafe { &mut *device };
    loop {
        while let Some((header, offset)) = gip::decode_header(&device.pending) {
            let end = offset + header.packet_length as usize;
            if end > device.pending.len() {
                break;
            }
            let payload: Vec<u8> = device.pending[offset..end].to_vec();
            device.pending.drain(..end);

            if header.command == gip::CMD_VIRTUAL_KEY {
                if let [down, key] = payload[..] {
                    if key == gip::VKEY_GUIDE {
                        device.guide_pressed = down != 0;
                    }
                }
            } else if header.command == gip::CMD_INPUT {
                if let Some(state) = gip::input::parse_input_report(&payload) {
                    let mut state: GipState = state.into();
                    state.buttons.guide = device.guide_pressed;
                    unsafe {
                        *out = state;
                    }
                    return GipPollResult::State;
                }
            }
        }

        let n = match device.reader.read(&mut device.buf) {
            Ok(n) => n,
            Err(e) if e.kind() == std::io::ErrorKind::TimedOut => return GipPollResult::Timeout,
            Err(e) => {
                log::warn!("gip_poll_state: read error: {e}");
                return GipPollResult::Error;
            }
        };
        if n == 0 {
            return GipPollResult::Error;
        }
        device.pending.extend_from_slice(&device.buf[..n]);
    }
}

/// Sends a rumble command to the physical controller's main motors.
/// `device` must be non-null and valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gip_send_rumble(device: *mut GipDevice, left: u8, right: u8) -> bool {
    let device = unsafe { &mut *device };
    gip::rumble::rumble(&mut device.writer, left, right).is_ok()
}

/// Frees a device opened by `gip_open_matching_device`. `device` may be
/// NULL (no-op).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gip_close(device: *mut GipDevice) {
    if !device.is_null() {
        drop(unsafe { Box::from_raw(device) });
    }
}

/// Returns the USB vendor ID of the connected controller. `device` must be
/// non-null and valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gip_device_vendor_id(device: *mut GipDevice) -> u16 {
    unsafe { &*device }.usb.vendor_id
}

/// Returns the USB product ID of the connected controller. `device` must be
/// non-null and valid.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gip_device_product_id(device: *mut GipDevice) -> u16 {
    unsafe { &*device }.usb.product_id
}
