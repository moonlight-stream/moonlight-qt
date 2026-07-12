use anyhow::{bail, Context, Result};
use nusb::transfer::{Direction, Interrupt, In, Out};
use nusb::MaybeFuture;

/// Vendor IDs of manufacturers known to ship Microsoft-licensed Xbox
/// accessories using the GIP protocol, per xone's `transport/wired.c`
/// device table (`xone_wired_id_table`).
const KNOWN_VENDOR_IDS: &[u16] = &[
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
];

// GIP's wired USB interface signature, per xone's `XONE_WIRED_VENDOR` macro:
// vendor-specific class, subclass 0x47, protocol 0xd0, on interface 0 (the
// data interface; audio, when present, is interface 1).
const GIP_INTERFACE_CLASS: u8 = 0xff;
const GIP_INTERFACE_SUBCLASS: u8 = 0x47;
const GIP_INTERFACE_PROTOCOL: u8 = 0xd0;
const GIP_DATA_INTERFACE_NUMBER: u8 = 0;

/// An opened GIP controller: the claimed USB interface plus the addresses of
/// its interrupt IN/OUT endpoints, discovered from the configuration
/// descriptor rather than hardcoded.
pub struct GipUsb {
    pub interface: nusb::Interface,
    pub ep_in: u8,
    pub ep_out: u8,
    pub vendor_id: u16,
    pub product_id: u16,
}

impl GipUsb {
    /// Finds, opens and claims the first connected GIP controller.
    pub fn open() -> Result<Self> {
        let candidates: Vec<_> = nusb::list_devices()
            .wait()
            .context("listing USB devices")?
            .filter(|d| KNOWN_VENDOR_IDS.contains(&d.vendor_id()))
            .collect();

        for info in &candidates {
            match Self::try_open(info) {
                Ok(dev) => return Ok(dev),
                Err(e) => log::debug!(
                    "skipping {:04x}:{:04x}: {e:#}",
                    info.vendor_id(),
                    info.product_id()
                ),
            }
        }

        bail!("no GIP controller found - is one plugged in?")
    }

    fn try_open(info: &nusb::DeviceInfo) -> Result<Self> {
        let vendor_id = info.vendor_id();
        let product_id = info.product_id();

        log::info!(
            "found candidate device {vendor_id:04x}:{product_id:04x}: bus {} addr {}",
            info.bus_id(),
            info.device_address()
        );

        let device = info.open().wait().context("opening USB device")?;

        // Since no system driver claims this device, macOS sometimes leaves
        // it "unconfigured" (no SET_CONFIGURATION). Force the first
        // available configuration before reading the configuration
        // descriptor.
        if device.active_configuration().is_err() {
            let value = device
                .configurations()
                .next()
                .context("device has no configuration descriptor")?
                .configuration_value();
            log::info!("device unconfigured, applying configuration {value}");
            device
                .set_configuration(value)
                .wait()
                .context("applying SET_CONFIGURATION")?;
        }

        let config = device
            .active_configuration()
            .context("reading configuration descriptor")?;

        let mut found: Option<(u8, u8, u8)> = None; // (interface_number, ep_in, ep_out)
        for iface in config.interface_alt_settings() {
            let is_gip_data_interface = iface.class() == GIP_INTERFACE_CLASS
                && iface.subclass() == GIP_INTERFACE_SUBCLASS
                && iface.protocol() == GIP_INTERFACE_PROTOCOL
                && iface.interface_number() == GIP_DATA_INTERFACE_NUMBER;
            if !is_gip_data_interface {
                continue;
            }

            let mut ep_in = None;
            let mut ep_out = None;
            for ep in iface.endpoints() {
                if ep.transfer_type() != nusb::descriptors::TransferType::Interrupt {
                    continue;
                }
                match ep.direction() {
                    Direction::In => ep_in = Some(ep.address()),
                    Direction::Out => ep_out = Some(ep.address()),
                }
            }
            if let (Some(i), Some(o)) = (ep_in, ep_out) {
                found = Some((iface.interface_number(), i, o));
                break;
            }
        }

        let (interface_number, ep_in, ep_out) =
            found.context("no GIP data interface (class 0xff/0x47/0xd0) found")?;

        log::info!(
            "using interface {interface_number}, endpoint IN 0x{ep_in:02x}, endpoint OUT 0x{ep_out:02x}"
        );

        let interface = device
            .claim_interface(interface_number)
            .wait()
            .context("claiming USB interface - is another process/driver using the device?")?;

        Ok(Self {
            interface,
            ep_in,
            ep_out,
            vendor_id,
            product_id,
        })
    }

    pub fn reader(&self, buf_size: usize) -> nusb::io::EndpointRead<Interrupt> {
        self.interface
            .endpoint::<Interrupt, In>(self.ep_in)
            .expect("valid IN endpoint")
            .reader(buf_size)
    }

    pub fn writer(&self, buf_size: usize) -> nusb::io::EndpointWrite<Interrupt> {
        self.interface
            .endpoint::<Interrupt, Out>(self.ep_out)
            .expect("valid OUT endpoint")
            .writer(buf_size)
    }
}
