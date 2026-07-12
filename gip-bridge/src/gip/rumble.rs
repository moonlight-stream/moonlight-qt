use super::{encode_packet, CMD_RUMBLE};
use anyhow::Result;
use std::io::Write;

/// Sends a rumble command driving the controller's main left/right motors
/// (0..255 each). Payload layout per xone's `gip_gamepad_pkt_rumble`; unlike
/// power/LED/identify, rumble is a plain client command (no `OPT_INTERNAL`).
pub fn rumble<W: Write>(mut writer: W, left: u8, right: u8) -> Result<()> {
    const MOTOR_LEFT_RIGHT: u8 = 0x03; // GIP_GP_MOTOR_R | GIP_GP_MOTOR_L
    let payload = [
        0x00,             // unknown
        MOTOR_LEFT_RIGHT, // motors bitmask
        0x00,             // left_trigger motor
        0x00,             // right_trigger motor
        left,
        right,
        0xff, // duration (max)
        0x00, // delay
        0xeb, // repeat
    ];
    let packet = encode_packet(CMD_RUMBLE, 0, 2, &payload);
    writer.write_all(&packet)?;
    writer.flush()?;
    Ok(())
}
