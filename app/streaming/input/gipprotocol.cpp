#include "gipprotocol.h"

namespace GipProtocol {

namespace {

void encodeVarint(std::vector<uint8_t>& buf, uint32_t val)
{
    for (;;) {
        uint8_t byte = val & 0x7f;
        val >>= 7;
        if (val != 0) {
            byte |= 0x80;
        }
        buf.push_back(byte);
        if (val == 0) {
            break;
        }
    }
}

// Returns the decoded value and the number of bytes consumed (0 if data is empty).
std::pair<uint32_t, size_t> decodeVarint(const uint8_t* data, size_t length)
{
    uint32_t val = 0;
    size_t i = 0;
    while (i < length && i < 4) {
        val |= static_cast<uint32_t>(data[i] & 0x7f) << (i * 7);
        bool more = (data[i] & 0x80) != 0;
        i++;
        if (!more) {
            break;
        }
    }
    return {val, i};
}

uint16_t readU16LE(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

} // namespace

bool decodeHeader(const uint8_t* data, size_t length, Header& header, size_t& payloadOffset)
{
    if (length < 4) {
        return false;
    }

    header.command = data[0];
    header.options = data[1];
    header.sequence = data[2];

    size_t offset = 3;
    auto [packetLength, varintLen] = decodeVarint(data + offset, length - offset);
    header.packetLength = packetLength;
    offset += varintLen;

    if (header.options & OPT_CHUNK) {
        auto [chunkOffset, chunkVarintLen] = decodeVarint(data + offset, length - offset);
        (void)chunkOffset; // Chunk reassembly isn't needed for the gamepad input path.
        offset += chunkVarintLen;
    }

    payloadOffset = offset;
    return true;
}

std::vector<uint8_t> encodePacket(uint8_t command, uint8_t options, uint8_t sequence,
                                   const std::vector<uint8_t>& payload)
{
    std::vector<uint8_t> buf = {command, options, sequence};
    encodeVarint(buf, static_cast<uint32_t>(payload.size()));

    // Header length must be even: if the varint above left it odd, mark its
    // last byte as "continued" and append a zero byte to pad it out. The
    // decoder's continuation-bit walk absorbs this transparently.
    if (buf.size() % 2 != 0) {
        buf.back() |= 0x80;
        buf.push_back(0);
    }

    buf.insert(buf.end(), payload.begin(), payload.end());
    return buf;
}

bool parseInputReport(const uint8_t* payload, size_t length, InputState& state)
{
    if (length < 14) {
        return false;
    }

    uint16_t buttons = readU16LE(payload + 0);

    state.menu = buttons & (1 << 2);
    state.view = buttons & (1 << 3);
    state.a = buttons & (1 << 4);
    state.b = buttons & (1 << 5);
    state.x = buttons & (1 << 6);
    state.y = buttons & (1 << 7);
    state.dpadUp = buttons & (1 << 8);
    state.dpadDown = buttons & (1 << 9);
    state.dpadLeft = buttons & (1 << 10);
    state.dpadRight = buttons & (1 << 11);
    state.bumperLeft = buttons & (1 << 12);
    state.bumperRight = buttons & (1 << 13);
    state.stickLeftClick = buttons & (1 << 14);
    state.stickRightClick = buttons & (1 << 15);

    state.triggerLeft = readU16LE(payload + 2);
    state.triggerRight = readU16LE(payload + 4);
    state.stickLeftX = static_cast<int16_t>(readU16LE(payload + 6));
    // Y axes are bitwise-negated to match xone's own convention (it feeds
    // ~raw to input_report_abs), so "up" comes out positive here too.
    state.stickLeftY = static_cast<int16_t>(~readU16LE(payload + 8));
    state.stickRightX = static_cast<int16_t>(readU16LE(payload + 10));
    state.stickRightY = static_cast<int16_t>(~readU16LE(payload + 12));

    return true;
}

bool parseVirtualKey(const uint8_t* payload, size_t length, bool& guidePressed)
{
    if (length < 2) {
        return false;
    }
    uint8_t down = payload[0];
    uint8_t key = payload[1];
    if (key != VKEY_GUIDE) {
        return false;
    }
    guidePressed = down != 0;
    return true;
}

std::vector<uint8_t> buildPowerOnPacket()
{
    return encodePacket(CMD_POWER, OPT_INTERNAL, 1, {POWER_MODE_ON});
}

std::vector<uint8_t> buildLedOnPacket(uint8_t brightness)
{
    return encodePacket(CMD_LED, OPT_INTERNAL, 3, {0x00, LED_MODE_ON, brightness});
}

std::vector<uint8_t> buildRumblePacket(uint8_t left, uint8_t right, uint8_t sequence)
{
    constexpr uint8_t MOTOR_LEFT_RIGHT = 0x03; // GIP_GP_MOTOR_R | GIP_GP_MOTOR_L
    std::vector<uint8_t> payload = {
        0x00,             // unknown
        MOTOR_LEFT_RIGHT, // motors bitmask
        0x00,             // left_trigger motor
        0x00,             // right_trigger motor
        left,
        right,
        0xff, // duration (max)
        0x00, // delay
        0xeb, // repeat
    };
    return encodePacket(CMD_RUMBLE, 0, sequence, payload);
}

std::vector<uint8_t> buildAckPacket(uint8_t ackedCommand, uint8_t sequence, uint16_t ackedPacketLength)
{
    // Layout per xone's `struct gip_pkt_acknowledge`: unknown(1), command(1),
    // options(1), length(le16), padding(2), remaining(le16) = 9 bytes.
    std::vector<uint8_t> payload = {
        0x00,          // unknown
        ackedCommand,  // command being acknowledged
        OPT_INTERNAL,  // options (no per-client-id bits to OR in here)
        static_cast<uint8_t>(ackedPacketLength & 0xff),
        static_cast<uint8_t>((ackedPacketLength >> 8) & 0xff),
        0x00, 0x00, // padding
        0x00, 0x00, // remaining (unchunked)
    };
    return encodePacket(CMD_ACKNOWLEDGE, OPT_INTERNAL, sequence, payload);
}

} // namespace GipProtocol
