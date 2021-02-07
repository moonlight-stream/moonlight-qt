#include "input.h"

#include <QString>

int SdlInputHandler::clipboardThreadProc(void *ptr)
{
    auto me = (SdlInputHandler*)ptr;

    while (!SDL_AtomicGet(&me->m_ShutdownClipboardThread)) {
        QString clipboardData;

        SDL_LockMutex(me->m_ClipboardLock);
        for (;;) {
            clipboardData = me->m_ClipboardData;
            me->m_ClipboardData.clear();
            if (!clipboardData.isEmpty() || SDL_AtomicGet(&me->m_ShutdownClipboardThread)) {
                break;
            }
            SDL_CondWait(me->m_ClipboardHasData, me->m_ClipboardLock);
        }
        SDL_UnlockMutex(me->m_ClipboardLock);

        // We might get here on shutdown, so don't send text if there's
        // nothing to send.
        if (!clipboardData.isEmpty()) {
            me->sendText(clipboardData);
        }
    }

    return 0;
}

#define MAP_KEY(c, sc) \
    case c: \
        event.key.keysym.scancode = sc; \
        break

#define MAP_KEY_SHIFT(c, sc) \
    case c: \
        event.key.keysym.scancode = sc; \
        event.key.keysym.mod = KMOD_SHIFT; \
        break

void SdlInputHandler::sendText(QString& string)
{
    for (int i = 0; i < string.size(); i++) {
        char16_t c = string[i].unicode();
        SDL_Event event = {};

        // If we're sending a very long string, we might get a termination request
        // while we're in the middle of sending a string. In that case, just bail.
        if (SDL_AtomicGet(&m_ShutdownClipboardThread)) {
            return;
        }

        if (c >= 'A' && c <= 'Z') {
            event.key.keysym.scancode = (SDL_Scancode)((c - 'A') + SDL_SCANCODE_A);
            event.key.keysym.mod = KMOD_SHIFT;
        }
        else if (c >= 'a' && c <= 'z') {
            event.key.keysym.scancode = (SDL_Scancode)((c - 'a') + SDL_SCANCODE_A);
        }
        else if (c >= '1' && c <= '9') {
            event.key.keysym.scancode = (SDL_Scancode)((c - '1') + SDL_SCANCODE_1);
        }
        else {
            // Fixup some Unicode characters to be expressible in ASCII
            switch (c) {
            case u'\U0000201C': // Left "
            case u'\U0000201D': // Right "
                c = '"';
                break;

            case u'\U00002018': // Left '
            case u'\U00002019': // Right '
                c = '\'';
                break;

            case u'\U00002013': // Smart -
                c = '-';
                break;

            case u'\U00002026': // Ellipsis (...)
                // Convert this character into 3 periods
                string.insert(i+1, '.');
                string.insert(i+1, '.');
                c = '.';
                break;

            case u'\U000000A0': // Non-breaking space
                c = ' ';
                break;

            default:
                // Nothing
                break;
            }


            switch (c) {

            // Handle CRLF separately to avoid duplicate newlines
            case '\r':
                if (string[i + 1] == '\n') {
                    i++;
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
                            "Pasting text - non-ASCII character value 'U+%x' ignored",
                            c);
                continue;
            }
        }

        if (event.key.keysym.mod & KMOD_SHIFT) {
            SDL_Event modifierEvent = {};
            modifierEvent.type = SDL_KEYDOWN;
            modifierEvent.key.state = SDL_PRESSED;
            modifierEvent.key.keysym.scancode = SDL_SCANCODE_LSHIFT;
            handleKeyEvent(&modifierEvent.key);

            SDL_Delay(10);
        }

        event.type = SDL_KEYDOWN;
        event.key.state = SDL_PRESSED;
        handleKeyEvent(&event.key);

        SDL_Delay(10);

        event.type = SDL_KEYUP;
        event.key.state = SDL_RELEASED;
        handleKeyEvent(&event.key);

        if (event.key.keysym.mod & KMOD_SHIFT) {
            SDL_Event modifierEvent = {};
            modifierEvent.type = SDL_KEYUP;
            modifierEvent.key.state = SDL_RELEASED;
            modifierEvent.key.keysym.scancode = SDL_SCANCODE_LSHIFT;
            handleKeyEvent(&modifierEvent.key);

            SDL_Delay(10);
        }
    }
}
