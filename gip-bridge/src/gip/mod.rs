pub mod handshake;
pub mod input;
pub mod led;
pub mod rumble;

pub const CMD_POWER: u8 = 0x05;
pub const CMD_VIRTUAL_KEY: u8 = 0x07;
pub const CMD_LED: u8 = 0x0a;
pub const CMD_RUMBLE: u8 = 0x09;
pub const CMD_INPUT: u8 = 0x20;

pub const OPT_INTERNAL: u8 = 1 << 5;
pub const OPT_CHUNK: u8 = 1 << 7;

pub const POWER_MODE_ON: u8 = 0x00;

pub const LED_MODE_ON: u8 = 0x01;
pub const LED_BRIGHTNESS_DEFAULT: u8 = 20;

/// The guide/Xbox button's key code in `GIP_CMD_VIRTUAL_KEY` payloads. It is
/// reported separately from the rest of the buttons in `GIP_CMD_INPUT`.
pub const VKEY_GUIDE: u8 = 0x5b;

/// A decoded GIP packet header. Layout per [MS-GIPUSB] / xone's
/// `bus/protocol.c`: command, options, sequence, then a LEB128-style
/// varint packet length (padded to keep the header an even number of
/// bytes), followed by a chunk-offset varint when `OPT_CHUNK` is set.
#[derive(Debug, Clone, Copy)]
pub struct GipHeader {
    pub command: u8,
    pub options: u8,
    pub sequence: u8,
    pub packet_length: u32,
}

/// Encodes a single non-chunked GIP packet (header + payload). Outbound
/// commands here are always small (power-on, rumble), so chunking is not
/// implemented on the encode side.
pub fn encode_packet(command: u8, options: u8, sequence: u8, payload: &[u8]) -> Vec<u8> {
    let mut buf = vec![command, options, sequence];
    encode_varint(&mut buf, payload.len() as u32);
    // Header length must be even: if the varint above left it odd, mark its
    // last byte as "continued" and append a zero byte to pad it out. The
    // decoder's continuation-bit walk absorbs this transparently.
    if buf.len() % 2 != 0 {
        let last = buf.len() - 1;
        buf[last] |= 0x80;
        buf.push(0);
    }
    buf.extend_from_slice(payload);
    buf
}

fn encode_varint(buf: &mut Vec<u8>, mut val: u32) {
    loop {
        let mut byte = (val & 0x7f) as u8;
        val >>= 7;
        if val != 0 {
            byte |= 0x80;
        }
        buf.push(byte);
        if val == 0 {
            break;
        }
    }
}

fn decode_varint(data: &[u8]) -> (u32, usize) {
    let mut val = 0u32;
    let mut i = 0;
    while i < data.len() && i < 4 {
        val |= ((data[i] & 0x7f) as u32) << (i * 7);
        let more = data[i] & 0x80 != 0;
        i += 1;
        if !more {
            break;
        }
    }
    (val, i)
}

/// Decodes one GIP packet header from the start of `data`. Returns the
/// header and the byte offset where its payload begins; the payload itself
/// is `data[offset..offset + header.packet_length]`.
pub fn decode_header(data: &[u8]) -> Option<(GipHeader, usize)> {
    if data.len() < 4 {
        return None;
    }
    let command = data[0];
    let options = data[1];
    let sequence = data[2];
    let mut offset = 3;

    let (packet_length, len) = decode_varint(&data[offset..]);
    offset += len;

    if options & OPT_CHUNK != 0 {
        let (_chunk_offset, len) = decode_varint(&data[offset..]);
        offset += len;
    }

    Some((
        GipHeader {
            command,
            options,
            sequence,
            packet_length,
        },
        offset,
    ))
}
