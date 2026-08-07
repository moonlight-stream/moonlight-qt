#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// GIP (Gaming Input Protocol) packet framing and payload parsing, per
// [MS-GIPUSB] and cross-referenced against the Linux `xone` driver
// (bus/protocol.c, driver/gamepad.c) for the parts Microsoft's spec leaves
// to interpretation.
namespace GipProtocol {

constexpr uint8_t CMD_ACKNOWLEDGE = 0x01;
constexpr uint8_t CMD_POWER = 0x05;
constexpr uint8_t CMD_VIRTUAL_KEY = 0x07;
constexpr uint8_t CMD_RUMBLE = 0x09;
constexpr uint8_t CMD_LED = 0x0a;
constexpr uint8_t CMD_INPUT = 0x20;

constexpr uint8_t OPT_ACKNOWLEDGE = 1 << 4;
constexpr uint8_t OPT_INTERNAL = 1 << 5;
constexpr uint8_t OPT_CHUNK = 1 << 7;

constexpr uint8_t POWER_MODE_ON = 0x00;

constexpr uint8_t LED_MODE_ON = 0x01;
constexpr uint8_t LED_BRIGHTNESS_DEFAULT = 20;

// The guide/Xbox button's key code in GIP_CMD_VIRTUAL_KEY payloads. It is
// reported separately from the rest of the buttons in GIP_CMD_INPUT.
constexpr uint8_t VKEY_GUIDE = 0x5b;

// A decoded GIP packet header: command, options, sequence, then a
// LEB128-style varint packet length (padded to keep the header an even
// number of bytes), followed by a chunk-offset varint when OPT_CHUNK is set.
struct Header
{
    uint8_t command;
    uint8_t options;
    uint8_t sequence;
    uint32_t packetLength;
};

// Decodes one GIP packet header from the start of data. On success, fills
// header and payloadOffset (data[payloadOffset..payloadOffset+packetLength]
// is the payload) and returns true.
bool decodeHeader(const uint8_t* data, size_t length, Header& header, size_t& payloadOffset);

// Encodes a single non-chunked GIP packet (header + payload). Outbound
// commands here are always small (power-on, LED, rumble), so chunking is
// not implemented on the encode side.
std::vector<uint8_t> encodePacket(uint8_t command, uint8_t options, uint8_t sequence,
                                   const std::vector<uint8_t>& payload);

// Parsed state of a GIP_CMD_INPUT (0x20) report, plus the guide button
// (carried in separately from GIP_CMD_VIRTUAL_KEY - see parseVirtualKey).
struct InputState
{
    bool menu = false;
    bool view = false;
    bool a = false;
    bool b = false;
    bool x = false;
    bool y = false;
    bool dpadUp = false;
    bool dpadDown = false;
    bool dpadLeft = false;
    bool dpadRight = false;
    bool bumperLeft = false;
    bool bumperRight = false;
    bool stickLeftClick = false;
    bool stickRightClick = false;
    bool guide = false;

    uint16_t triggerLeft = 0;
    uint16_t triggerRight = 0;
    int16_t stickLeftX = 0;
    int16_t stickLeftY = 0;
    int16_t stickRightX = 0;
    int16_t stickRightY = 0;
};

// Parses a GIP_CMD_INPUT payload, per xone's `gip_gamepad_pkt_input` layout:
// 7 little-endian u16 fields (buttons, 2 triggers, 4 stick axes). Leaves
// `guide` untouched - callers should carry that field forward themselves
// from the last parseVirtualKey() result, since it isn't part of this
// report. Returns false if payload is too short.
bool parseInputReport(const uint8_t* payload, size_t length, InputState& state);

// Parses a GIP_CMD_VIRTUAL_KEY payload. Returns true and fills guidePressed
// if this packet is for the guide/Xbox key; false otherwise (payload too
// short, or a different key).
bool parseVirtualKey(const uint8_t* payload, size_t length, bool& guidePressed);

// Wakes the controller up. Wired GIP controllers (Xbox One/Series-generation,
// which is what these third-party pads emulate) stay completely silent on
// the interrupt IN endpoint until the host sends this - the interface claims
// fine and the OS enumerates it normally, but no input reports ever arrive
// without it.
std::vector<uint8_t> buildPowerOnPacket();

// Turns the controller's LED solid on. Without this, the controller stays
// in its firmware-default blinking "searching for connection" pattern even
// though it is fully connected and streaming input - the LED state is never
// inferred from traffic, only ever set explicitly by the host.
std::vector<uint8_t> buildLedOnPacket(uint8_t brightness);

// Builds a rumble command driving the controller's main left/right motors
// (0..255 each). Payload layout per xone's `gip_gamepad_pkt_rumble`; unlike
// power/LED, rumble is a plain client command (no OPT_INTERNAL).
//
// sequence must be different from the previous rumble packet sent to the
// same device (and should keep changing on every call, e.g. a counter the
// caller increments each time) - GIP treats a repeated sequence number as a
// retransmission of an already-handled command, so reusing one would cause
// the controller to silently ignore a later call, including a left=0/
// right=0 "stop" meant to cancel an earlier effect's programmed duration.
std::vector<uint8_t> buildRumblePacket(uint8_t left, uint8_t right, uint8_t sequence);

// Builds a GIP_CMD_ACKNOWLEDGE reply for an incoming packet that had
// OPT_ACKNOWLEDGE set in its header. Per xone's `gip_acknowledge_pkt`/
// `gip_pkt_acknowledge`: the reply echoes the acknowledged command and
// packet length back at the sequence number of the packet being
// acknowledged. Devices that request an ack and don't get one will keep
// retransmitting the unacknowledged packet (observed on hardware as a GIP
// guide-button VIRTUAL_KEY report resent roughly every 70-80ms until the
// controller gives up, re-triggering its own local haptic pulse on every
// retry). `remaining` is always sent as 0 here - chunk reassembly isn't
// implemented (see decodeHeader), and none of the packet types this bridge
// receives (input reports, virtual keys) are ever chunked in practice.
std::vector<uint8_t> buildAckPacket(uint8_t ackedCommand, uint8_t sequence, uint16_t ackedPacketLength);

} // namespace GipProtocol
