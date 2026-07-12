/// Parsed state of a `GIP_CMD_INPUT` (0x20) report.
#[derive(Debug, Default, Clone, Copy, PartialEq, Eq)]
pub struct GamepadState {
    pub menu: bool,
    pub view: bool,
    pub a: bool,
    pub b: bool,
    pub x: bool,
    pub y: bool,
    pub dpad_up: bool,
    pub dpad_down: bool,
    pub dpad_left: bool,
    pub dpad_right: bool,
    pub bumper_left: bool,
    pub bumper_right: bool,
    pub stick_left_click: bool,
    pub stick_right_click: bool,
    pub trigger_left: u16,
    pub trigger_right: u16,
    pub stick_left_x: i16,
    pub stick_left_y: i16,
    pub stick_right_x: i16,
    pub stick_right_y: i16,
}

const BTN_MENU: u16 = 1 << 2;
const BTN_VIEW: u16 = 1 << 3;
const BTN_A: u16 = 1 << 4;
const BTN_B: u16 = 1 << 5;
const BTN_X: u16 = 1 << 6;
const BTN_Y: u16 = 1 << 7;
const BTN_DPAD_UP: u16 = 1 << 8;
const BTN_DPAD_DOWN: u16 = 1 << 9;
const BTN_DPAD_LEFT: u16 = 1 << 10;
const BTN_DPAD_RIGHT: u16 = 1 << 11;
const BTN_BUMPER_LEFT: u16 = 1 << 12;
const BTN_BUMPER_RIGHT: u16 = 1 << 13;
const BTN_STICK_LEFT: u16 = 1 << 14;
const BTN_STICK_RIGHT: u16 = 1 << 15;

/// Parses a `GIP_CMD_INPUT` payload, per xone's `gip_gamepad_pkt_input`
/// layout: 7 little-endian u16 fields (buttons, 2 triggers, 4 stick axes).
/// The Y axes are bitwise-negated to match xone's own convention (it feeds
/// `~raw` to `input_report_abs`), so "up" comes out positive here too.
pub fn parse_input_report(payload: &[u8]) -> Option<GamepadState> {
    if payload.len() < 14 {
        return None;
    }
    let u16_at = |i: usize| u16::from_le_bytes([payload[i], payload[i + 1]]);
    let buttons = u16_at(0);
    Some(GamepadState {
        menu: buttons & BTN_MENU != 0,
        view: buttons & BTN_VIEW != 0,
        a: buttons & BTN_A != 0,
        b: buttons & BTN_B != 0,
        x: buttons & BTN_X != 0,
        y: buttons & BTN_Y != 0,
        dpad_up: buttons & BTN_DPAD_UP != 0,
        dpad_down: buttons & BTN_DPAD_DOWN != 0,
        dpad_left: buttons & BTN_DPAD_LEFT != 0,
        dpad_right: buttons & BTN_DPAD_RIGHT != 0,
        bumper_left: buttons & BTN_BUMPER_LEFT != 0,
        bumper_right: buttons & BTN_BUMPER_RIGHT != 0,
        stick_left_click: buttons & BTN_STICK_LEFT != 0,
        stick_right_click: buttons & BTN_STICK_RIGHT != 0,
        trigger_left: u16_at(2),
        trigger_right: u16_at(4),
        stick_left_x: u16_at(6) as i16,
        stick_left_y: !(u16_at(8) as i16),
        stick_right_x: u16_at(10) as i16,
        stick_right_y: !(u16_at(12) as i16),
    })
}
