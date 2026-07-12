use super::{encode_packet, CMD_LED, LED_MODE_ON, OPT_INTERNAL};
use anyhow::Result;
use std::io::Write;

/// Turns the controller's LED solid on. Without this, the controller stays
/// in its firmware-default blinking "searching for connection" pattern even
/// though it is fully connected and streaming input - the LED state is
/// never inferred from traffic, only ever set explicitly by the host.
pub fn led_on<W: Write>(mut writer: W, brightness: u8) -> Result<()> {
    let payload = [0x00, LED_MODE_ON, brightness];
    let packet = encode_packet(CMD_LED, OPT_INTERNAL, 3, &payload);
    writer.write_all(&packet)?;
    writer.flush()?;
    Ok(())
}
