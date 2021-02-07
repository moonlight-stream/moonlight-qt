#include "input.h"

#define MAP_KEY(c, sc) \
    case c: \
        event.key.keysym.scancode = sc; \
        break

#define MAP_KEY_SHIFT(c, sc) \
    case c: \
        event.key.keysym.scancode = sc; \
        event.key.keysym.mod = KMOD_SHIFT; \
        break

void SdlInputHandler::sendText(const char* text)
{
    for (const char* c = text; *c != 0; c++) {
        SDL_Event event = {};

        if (*c >= 'A' && *c <= 'Z') {
            event.key.keysym.scancode = (SDL_Scancode)((*c - 'A') + SDL_SCANCODE_A);
            event.key.keysym.mod = KMOD_SHIFT;
        }
        else if (*c >= 'a' && *c <= 'z') {
            event.key.keysym.scancode = (SDL_Scancode)((*c - 'a') + SDL_SCANCODE_A);
        }
        else if (*c >= '1' && *c <= '9') {
            event.key.keysym.scancode = (SDL_Scancode)((*c - '1') + SDL_SCANCODE_1);
        }
        else {
            // TODO: Smartquotes
            switch (*c) {

            // Handle CRLF separately to avoid duplicate newlines
            case '\r':
                if (*(c + 1) == '\n') {
                    c++;
                }
                event.key.keysym.scancode = SDL_SCANCODE_RETURN;
                break;

            MAP_KEY('\b', SDL_SCANCODE_BACKSPACE);
            MAP_KEY('\n', SDL_SCANCODE_RETURN);
            MAP_KEY('\t', SDL_SCANCODE_TAB);

            MAP_KEY(' ', SDL_SCANCODE_SPACE);
            MAP_KEY_SHIFT('!', SDL_SCANCODE_1);
            MAP_KEY_SHIFT('"', SDL_SCANCODE_APOSTROPHE);
            MAP_KEY_SHIFT('#', SDL_SCANCODE_3);
            MAP_KEY_SHIFT('$', SDL_SCANCODE_4);
            MAP_KEY_SHIFT('%', SDL_SCANCODE_5);
            MAP_KEY_SHIFT('&', SDL_SCANCODE_7);
            MAP_KEY('\'', SDL_SCANCODE_APOSTROPHE);
            MAP_KEY_SHIFT('(', SDL_SCANCODE_9);
            MAP_KEY_SHIFT(')', SDL_SCANCODE_0);
            MAP_KEY_SHIFT('*', SDL_SCANCODE_8);
            MAP_KEY_SHIFT('+', SDL_SCANCODE_EQUALS);
            MAP_KEY(',', SDL_SCANCODE_COMMA);
            MAP_KEY('-', SDL_SCANCODE_MINUS);
            MAP_KEY('.', SDL_SCANCODE_PERIOD);
            MAP_KEY('/', SDL_SCANCODE_SLASH);
            MAP_KEY('0', SDL_SCANCODE_0);

            MAP_KEY_SHIFT(':', SDL_SCANCODE_SEMICOLON);
            MAP_KEY(';', SDL_SCANCODE_SEMICOLON);
            MAP_KEY_SHIFT('<', SDL_SCANCODE_COMMA);
            MAP_KEY('=', SDL_SCANCODE_EQUALS);
            MAP_KEY_SHIFT('>', SDL_SCANCODE_PERIOD);
            MAP_KEY_SHIFT('?', SDL_SCANCODE_SLASH);
            MAP_KEY_SHIFT('@', SDL_SCANCODE_2);

            MAP_KEY('[', SDL_SCANCODE_LEFTBRACKET);
            MAP_KEY('\\', SDL_SCANCODE_BACKSLASH);
            MAP_KEY(']', SDL_SCANCODE_RIGHTBRACKET);
            MAP_KEY_SHIFT('^', SDL_SCANCODE_6);
            MAP_KEY_SHIFT('_', SDL_SCANCODE_MINUS);
            MAP_KEY('`', SDL_SCANCODE_GRAVE);

            MAP_KEY_SHIFT('{', SDL_SCANCODE_LEFTBRACKET);
            MAP_KEY_SHIFT('|', SDL_SCANCODE_BACKSLASH);
            MAP_KEY_SHIFT('}', SDL_SCANCODE_RIGHTBRACKET);
            MAP_KEY_SHIFT('~', SDL_SCANCODE_GRAVE);

            default:
                SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                            "Pasting text - non-ASCII character '%c' ignored",
                            *c);
                continue;
            }
        }

        event.type = SDL_KEYDOWN;
        event.key.state = SDL_PRESSED;
        handleKeyEvent(&event.key);

        SDL_Delay(10);

        event.type = SDL_KEYUP;
        event.key.state = SDL_RELEASED;
        handleKeyEvent(&event.key);
    }
}
