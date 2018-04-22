//
//  keyboardTranslation.m
//  Moonlight macOS
//
//  Created by Felix Kratz on 10.03.18.
//  Copyright © 2018 Felix Kratz. All rights reserved.
//

#import "keyboardTranslation.h"
#import <Limelight.h>

typedef enum {
    NOKEY,
    VK_BACKSPACE = 0x08,
    VK_TAB = 0x09,
    VK_ENTER = 0x0D,
    VK_SPACE = 0x20,
    VK_LEFT = 0x25,
    VK_UP,
    VK_RIGHT,
    VK_DOWN,
    VK_SHIFT = 0xA0,
    VK_CTRL = 0xA2,
    VK_ALT = 0xA4,
    VK_COMMA = 0xBC,
    VK_MINUS = 0xBD,
    VK_PERIOD = 0xBE,
    VK_ESC = 0x1B,
    VK_F1 = 0x70,
    VK_F2,
    VK_F3,
    VK_F4,
    VK_F5,
    VK_F6,
    VK_F7,
    VK_F8,
    VK_F9,
    VK_F10,
    VK_F11,
    VK_F12,
    VK_F13,
    VK_F14,
    VK_F15,
    VK_DEL = 0x7F,
} SpecialKeyCodes;

CGKeyCode keyCodeFromModifierKey(NSEventModifierFlags keyModifier) {
    if (keyModifier & kCGEventFlagMaskShift) {
        return VK_SHIFT;
    }
    if (keyModifier & kCGEventFlagMaskAlternate) {
        return VK_ALT;
    }
    if (keyModifier & kCGEventFlagMaskControl) {
        return VK_CTRL;
    }
    return NOKEY;
}

char modifierFlagForKeyModifier(NSEventModifierFlags keyModifier) {
    if (keyModifier & kCGEventFlagMaskShift) {
        return MODIFIER_SHIFT;
    }
    if (keyModifier & kCGEventFlagMaskAlternate) {
        return MODIFIER_ALT;
    }
    if (keyModifier & kCGEventFlagMaskControl) {
        return MODIFIER_CTRL;
    }
    return NOKEY;
}

CGKeyCode keyCharFromKeyCode(CGKeyCode keyCode) {
    switch (keyCode) {
        case kVK_ANSI_A: return 'A';
        case kVK_ANSI_S: return 'S';
        case kVK_ANSI_D: return 'D';
        case kVK_ANSI_F: return 'F';
        case kVK_ANSI_H: return 'H';
        case kVK_ANSI_G: return 'G';
        case kVK_ANSI_Y: return 'Y';
        case kVK_ANSI_X: return 'X';
        case kVK_ANSI_C: return 'C';
        case kVK_ANSI_V: return 'V';
        case kVK_ANSI_B: return 'B';
        case kVK_ANSI_Q: return 'Q';
        case kVK_ANSI_W: return 'W';
        case kVK_ANSI_E: return 'E';
        case kVK_ANSI_R: return 'R';
        case kVK_ANSI_Z: return 'Z';
        case kVK_ANSI_T: return 'T';
        case kVK_ANSI_1: return '1';
        case kVK_ANSI_2: return '2';
        case kVK_ANSI_3: return '3';
        case kVK_ANSI_4: return '4';
        case kVK_ANSI_6: return '6';
        case kVK_ANSI_5: return '5';
        case kVK_ANSI_9: return '9';
        case kVK_ANSI_7: return '7';
        case kVK_ANSI_8: return '8';
        case kVK_ANSI_0: return '0';
        case kVK_ANSI_O: return 'O';
        case kVK_ANSI_U: return 'U';
        case kVK_ANSI_I: return 'I';
        case kVK_ANSI_P: return 'P';
        case kVK_ANSI_L: return 'L';
        case kVK_ANSI_J: return 'J';
        case kVK_ANSI_K: return 'K';
        case kVK_ANSI_N: return 'N';
        case kVK_ANSI_M: return 'M';
        case kVK_Return: return VK_ENTER;
        case kVK_ANSI_Period: return VK_PERIOD;
        case kVK_ANSI_Comma: return VK_COMMA;
        case kVK_ANSI_Minus: return VK_MINUS;
        case kVK_Tab: return VK_TAB;
        case kVK_Space: return VK_SPACE;
        case kVK_Delete: return VK_BACKSPACE;
        case kVK_Escape: return VK_ESC;
        case kVK_F1: return VK_F1;
        case kVK_F2: return VK_F2;
        case kVK_F3: return VK_F3;
        case kVK_F4: return VK_F4;
        case kVK_F5: return VK_F5;
        case kVK_F6: return VK_F6;
        case kVK_F7: return VK_F7;
        case kVK_F8: return VK_F8;
        case kVK_F9: return VK_F9;
        case kVK_F10: return VK_F10;
        case kVK_F11: return VK_F11;
        case kVK_F12: return VK_F12;
        case kVK_F13: return VK_F13;
        case kVK_F14: return VK_F14;
        case kVK_F15: return VK_F15;
        case kVK_LeftArrow: return VK_LEFT;
        case kVK_RightArrow: return VK_RIGHT;
        case kVK_DownArrow: return VK_DOWN;
        case kVK_UpArrow: return VK_UP;
            
        default:
            return NOKEY;
    }
}
