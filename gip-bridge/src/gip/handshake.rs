use super::{encode_packet, CMD_POWER, OPT_INTERNAL, POWER_MODE_ON};
use anyhow::Result;
use std::io::Write;

/// Wakes the controller up. Wired GIP controllers (Xbox One/Series-generation,
/// which is what this 8BitDo pad emulates) stay completely silent on the
/// interrupt IN endpoint until the host sends this - the interface claims
/// fine and the OS enumerates it normally, but no input reports ever arrive
/// without it.
pub fn power_on<W: Write>(mut writer: W) -> Result<()> {
    let packet = encode_packet(CMD_POWER, OPT_INTERNAL, 1, &[POWER_MODE_ON]);
    writer.write_all(&packet)?;
    writer.flush()?;
    Ok(())
}
