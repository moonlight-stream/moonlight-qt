use anyhow::Result;
use gip_bridge::{gip, usb};
use std::io::Read;

/// Standalone debug tool: opens the controller, does the GIP handshake, and
/// logs parsed input reports to the terminal. Useful for validating the USB
/// + protocol layer in isolation from whatever embeds `gip_bridge::ffi`.
fn main() -> Result<()> {
    env_logger::Builder::from_env(env_logger::Env::default().default_filter_or("info")).init();

    let dev = usb::GipUsb::open()?;

    log::info!("sending power-on to the controller...");
    gip::handshake::power_on(dev.writer(64))?;

    log::info!("reading reports (ctrl+c to quit)...");
    let mut reader = dev.reader(64);
    let mut buf = [0u8; 64];
    let mut last_logged: Option<gip::input::GamepadState> = None;

    loop {
        let n = reader.read(&mut buf)?;
        if n == 0 {
            continue;
        }
        let mut data = &buf[..n];
        while let Some((header, offset)) = gip::decode_header(data) {
            let end = offset + header.packet_length as usize;
            if end > data.len() {
                break;
            }
            let payload = &data[offset..end];
            log::debug!(
                "pkt cmd=0x{:02x} opt=0x{:02x} seq={} len={}",
                header.command,
                header.options,
                header.sequence,
                header.packet_length
            );

            if header.command == gip::CMD_INPUT {
                if let Some(state) = gip::input::parse_input_report(payload) {
                    if last_logged != Some(state) {
                        log::info!("{state:?}");
                        last_logged = Some(state);
                    }
                }
            }

            data = &data[end..];
            if data.is_empty() {
                break;
            }
        }
    }
}
