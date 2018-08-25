//
//  KeyboardSupport.m
//  Moonlight
//
//  Created by Diego Waxemberg on 8/25/18.
//  Copyright © 2018 Moonlight Game Streaming Project. All rights reserved.
//

#import "KeyboardSupport.h"
#include <Limelight.h>

@implementation KeyboardSupport

+ (struct KeyEvent)translateKeyEvent:(unichar)inputChar {
    struct KeyEvent event;
    event.keycode = 0;
    event.modifier = 0;
    event.modifierKeycode = 0;
    
    if (inputChar >= 0x30 && inputChar <= 0x39) {
        // Numbers 0-9
        event.keycode = inputChar;
    } else if (inputChar >= 0x41 && inputChar <= 0x5A) {
        // Capital letters
        event.keycode = inputChar;
        [KeyboardSupport addShiftModifier:&event];
    } else if (inputChar >= 0x61 && inputChar <= 0x7A) {
        // Lower case letters
        event.keycode = inputChar - (0x61 - 0x41);
    } switch (inputChar) {
        case ' ': // Spacebar
            event.keycode = 0x20;
            break;
        case '-': // Hyphen '-'
            event.keycode = 0xBD;
            break;
        case '/': // Forward slash '/'
            event.keycode = 0xBF;
            break;
        case ':': // Colon ':'
            event.keycode = 0xBA;
            [KeyboardSupport addShiftModifier:&event];
            break;
        case ';': // Semi-colon ';'
            event.keycode = 0xBA;
            break;
        case '(': // Open parenthesis '('
            event.keycode = 0x39; // '9'
            [KeyboardSupport addShiftModifier:&event];
            break;
        case ')': // Close parenthesis ')'
            event.keycode = 0x30; // '0'
            [KeyboardSupport addShiftModifier:&event];
            break;
        case '$': // Dollar sign '$'
            event.keycode = 0x34; // '4'
            [KeyboardSupport addShiftModifier:&event];
            break;
        case '&': // Ampresand '&'
            event.keycode = 0x37; // '7'
            [KeyboardSupport addShiftModifier:&event];
            break;
        case '@': // At-sign '@'
            event.keycode = 0x32; // '2'
            [KeyboardSupport addShiftModifier:&event];
            break;
        case '"':
            event.keycode = 0xDE;
            [KeyboardSupport addShiftModifier:&event];
            break;
        case '\'':
            event.keycode = 0xDE;
            break;
        case '!':
            event.keycode = 0x31; // '1'
            [KeyboardSupport addShiftModifier:&event];
            break;
        case '?':
            event.keycode = 0xBF; // '/'
            [KeyboardSupport addShiftModifier:&event];
            break;
        case ',':
            event.keycode = 0xBC;
            break;
        case '<':
            event.keycode = 0xBC;
            [KeyboardSupport addShiftModifier:&event];
            break;
        case '.':
            event.keycode = 0xBE;
            break;
        case '>':
            event.keycode = 0xBE;
            [KeyboardSupport addShiftModifier:&event];
            break;
        case '[':
            event.keycode = 0xDB;
            break;
        case ']':
            event.keycode = 0xDD;
            break;
        case '{':
            event.keycode = 0xDB;
            [KeyboardSupport addShiftModifier:&event];
            break;
        case '}':
            event.keycode = 0xDD;
            [KeyboardSupport addShiftModifier:&event];
            break;
        case '#':
            event.keycode = 0x33; // '3'
            [KeyboardSupport addShiftModifier:&event];
            break;
        case '%':
            event.keycode = 0x35; // '5'
            [KeyboardSupport addShiftModifier:&event];
            break;
        case '^':
            event.keycode = 0x36; // '6'
            [KeyboardSupport addShiftModifier:&event];
            break;
        case '*':
            event.keycode = 0x38; // '8'
            [KeyboardSupport addShiftModifier:&event];
            break;
        case '+':
            event.keycode = 0xBB;
            [KeyboardSupport addShiftModifier:&event];
            break;
        case '=':
            event.keycode = 0xBB;
            break;
        case '_':
            event.keycode = 0xBD;
            [KeyboardSupport addShiftModifier:&event];
            break;
        case '\\':
            event.keycode = 0xDC;
            break;
        case '|':
            event.keycode = 0xDC;
            [KeyboardSupport addShiftModifier:&event];
            break;
        case '~':
            event.keycode = 0xC0;
            [KeyboardSupport addShiftModifier:&event];
            break;
        case '`':
            event.keycode = 0xC0;
            break;
        case '\t':
            event.keycode = 0x09;
            break;
        default:
            break;
    }
 
    return event;
}

+ (void) addShiftModifier:(struct KeyEvent*)event {
    event->modifier = MODIFIER_SHIFT;
    event->modifierKeycode = 0x10;
}

@end
