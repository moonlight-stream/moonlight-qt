#!/usr/bin/env python3
"""Optional Linux/Unix check of extracted keyboard code with inert SDL/host stubs.

Requires Python 3.9+, Git, pkg-config, GCC/Clang with C++17, SDL2 and Qt6 development
headers (Qt6Core and Qt6Qml), and the checked-out moonlight-common-c submodule.
Only QtCore is linked. No SDL library is linked, no window is created, and all
host input calls write to memory only.
The v6.2.0 tag must reproduce the stuck GUI-key regression; the working source
must pass. Temporary build files are removed when the command exits.

Run from a source checkout with the v6.2.0 tag available:
    git fetch origin tag v6.2.0
    git submodule update --init moonlight-common-c/moonlight-common-c
    python3 scripts/test-keyboard-toggle.py

This extracted-code check simplifies construction and mouse capture, forces
desktop detection true, and stubs SDL/network delivery. It does not replace a
full application build or live compositor/host integration test.
"""

from pathlib import Path
import argparse
import re
import shlex
import subprocess
import tempfile


def function(source: str, signature: str) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    closing = opening + 1
    while depth:
        depth += (source[closing] == "{") - (source[closing] == "}")
        closing += 1
    return source[start:closing]


def case(source: str, name: str) -> str:
    match = re.search(
        rf"    case {name}:\n.*?(?=\n    case |\n    default:)",
        source,
        re.S,
    )
    if not match:
        raise ValueError(f"Missing upstream case {name}")
    return match[0]


def harness(keyboard: str, input_source: str, input_header: str) -> str:
    definitions = keyboard[keyboard.index("#define VK_0"):keyboard.index(
        "void SdlInputHandler::performSpecialKeyCombo"
    )]
    selected_cases = "\n".join(case(keyboard, name) for name in (
        "KeyComboUngrabInput", "KeyComboToggleKeyboardGrab"
    ))
    methods = "\n\n".join((
        function(keyboard, "void SdlInputHandler::handleKeyEvent("),
        function(keyboard, "void SdlInputHandler::raiseAllKeys("),
        function(input_source, "bool SdlInputHandler::isCaptureActive("),
        function(input_source, "void SdlInputHandler::updateKeyboardGrabState("),
        function(input_source, "bool SdlInputHandler::isSystemKeyCaptureActive("),
        function(input_source, "void SdlInputHandler::notifyFocusLost("),
    ))
    combo_enum = re.search(r"    enum KeyCombo \{.*?^    \};", input_header, re.S | re.M)
    if not combo_enum:
        raise ValueError("Missing upstream KeyCombo enum")
    combo_configuration = "\n".join(
        line for line in input_source.splitlines()
        if re.match(r"    m_SpecialKeyCombos\[KeyCombo(?:UngrabInput|ToggleKeyboardGrab)\]", line)
    )
    if len(combo_configuration.splitlines()) != 8:
        raise ValueError("Expected four upstream assignments each for K and Z")
    handler_class = CLASS.replace("@KEY_COMBO_ENUM@", combo_enum[0]).replace(
        "@CONFIGURE_SHORTCUTS@", combo_configuration
    )
    return PREAMBLE + definitions + handler_class + "\n" + (
        "void SdlInputHandler::performSpecialKeyCombo(KeyCombo combo) {\n"
        "    switch (combo) {\n" + selected_cases + "\n    default: abort();\n    }\n}\n"
    ) + methods + TESTS


PREAMBLE = r'''
#define SDL_MAIN_HANDLED
#include "SDL_compat.h"
#include <Limelight.h>
#include <QSet>
#include "settings/streamingpreferences.h"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#undef SDL_assert
#define SDL_assert(condition) assert(condition)

struct SentKey { short code; char action, modifiers, flags; };
std::vector<SentKey> sent;
using HostKey = std::tuple<short, int, int>;
std::set<HostKey> hostKeys;
Uint32 windowFlags;
bool relativeMouse, keyboardGrab;
namespace WMUtils {
bool isRunningDesktopEnvironment() { return true; }
}

// These definitions satisfy SDL declarations without linking or calling SDL.
extern "C" Uint32 SDL_GetWindowFlags(SDL_Window*) { return windowFlags; }
extern "C" SDL_bool SDL_GetRelativeMouseMode() {
    return relativeMouse ? SDL_TRUE : SDL_FALSE;
}
extern "C" SDL_bool SDL_SetHint(const char*, const char*) { return SDL_TRUE; }
extern "C" void SDL_SetWindowKeyboardGrab(SDL_Window*, SDL_bool grab) {
    keyboardGrab = grab == SDL_TRUE;
}
extern "C" void SDL_LogInfo(int, const char*, ...) {}

extern "C" int LiSendKeyboardEvent2(short code, char action, char modifiers, char flags) {
    sent.push_back({code, action, modifiers, flags});
    const HostKey identity(code, modifiers & MODIFIER_EXTENDED, flags);
    if (action == KEY_ACTION_DOWN) hostKeys.insert(identity);
    else hostKeys.erase(identity);
    return 0;
}
'''


CLASS = r'''
class SdlInputHandler {
public:
@KEY_COMBO_ENUM@
    struct Combo { KeyCombo keyCombo; SDL_Keycode keyCode; SDL_Scancode scanCode;
                   bool enabled; };
    Combo m_SpecialKeyCombos[KeyComboMax]{};
    QSet<uint32_t> m_KeysDown;
    SDL_Window* m_Window = reinterpret_cast<SDL_Window*>(uintptr_t{1});
    bool m_FakeMouseCaptureActive = false, m_KeyboardCaptureActive = false;
    bool m_AbsoluteMouseMode = false;
    StreamingPreferences::CaptureSysKeysMode m_CaptureSystemKeysMode;

    SdlInputHandler(StreamingPreferences::CaptureSysKeysMode mode, bool capture) :
        m_CaptureSystemKeysMode(mode) {
        sent.clear();
        hostKeys.clear();
        windowFlags = SDL_WINDOW_INPUT_FOCUS | SDL_WINDOW_FULLSCREEN;
        relativeMouse = capture;
        keyboardGrab = false;
@CONFIGURE_SHORTCUTS@
        updateKeyboardGrabState();
    }
    void handleKeyEvent(SDL_KeyboardEvent*);
    void raiseAllKeys();
    void performSpecialKeyCombo(KeyCombo);
    bool isCaptureActive();
    bool isSystemKeyCaptureActive();
    void updateKeyboardGrabState();
    void notifyFocusLost();
    void setCaptureActive(bool capture) {
        relativeMouse = capture;
        updateKeyboardGrabState();
    }
};
'''


TESTS = r'''
int failures = 0;
void check(bool condition, const std::string& context) {
    if (!condition) { ++failures; std::cerr << "FAIL " << context << '\n'; }
}
void emitKey(SdlInputHandler& handler, SDL_Scancode scan, SDL_Keycode symbol,
             bool down, Uint16 modifiers, bool repeat = false) {
    SDL_KeyboardEvent event{};
    event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
    event.state = down ? SDL_PRESSED : SDL_RELEASED;
    event.repeat = repeat;
    event.keysym.scancode = scan;
    event.keysym.sym = symbol;
    event.keysym.mod = modifiers;
    handler.handleKeyEvent(&event);
}
constexpr Uint16 shortcutMods = KMOD_CTRL | KMOD_ALT | KMOD_SHIFT;
void pressModifiers(SdlInputHandler& handler, Uint16 extra) {
    emitKey(handler, SDL_SCANCODE_LCTRL, SDLK_LCTRL, true, KMOD_LCTRL | extra);
    emitKey(handler, SDL_SCANCODE_LALT, SDLK_LALT, true,
            KMOD_LCTRL | KMOD_LALT | extra);
    emitKey(handler, SDL_SCANCODE_LSHIFT, SDLK_LSHIFT, true,
            shortcutMods | extra);
}
void releaseModifiers(SdlInputHandler& handler, Uint16 extra) {
    emitKey(handler, SDL_SCANCODE_LCTRL, SDLK_LCTRL, false,
            KMOD_LALT | KMOD_LSHIFT | extra);
    emitKey(handler, SDL_SCANCODE_LALT, SDLK_LALT, false, KMOD_LSHIFT | extra);
    emitKey(handler, SDL_SCANCODE_LSHIFT, SDLK_LSHIFT, false, extra);
}
void disableWithHeldGui(SDL_Scancode gui, SDL_Keycode symbol, Uint16 guiMod,
                       StreamingPreferences::CaptureSysKeysMode mode,
                       bool scancodeFallback) {
    SdlInputHandler handler(mode, true);
    const short guiCode = short(0x8000 | (gui == SDL_SCANCODE_LGUI ? 0x5B : 0x5C));
    const std::string context = gui == SDL_SCANCODE_LGUI ? "LGUI" : "RGUI";
    emitKey(handler, gui, symbol, true, guiMod);
    pressModifiers(handler, guiMod);
    check(hostKeys.count(HostKey(guiCode, MODIFIER_EXTENDED, 0)) == 1 &&
          handler.m_KeysDown.size() == 4,
          context + " setup forwards GUI and shortcut modifiers");
    const size_t beforeToggle = sent.size();
    emitKey(handler, SDL_SCANCODE_K, scancodeFallback ? 0 : SDLK_k,
            true, shortcutMods | guiMod);
    check(handler.m_CaptureSystemKeysMode == StreamingPreferences::CSK_OFF &&
          !keyboardGrab && relativeMouse, context + " K releases only keyboard grab");
    check(hostKeys.empty(), context + " K releases all forwarded keys immediately");
    check(handler.m_KeysDown.isEmpty(), context + " K clears tracked key state");
    bool guiReleasedWithFlags = false;
    for (size_t i = beforeToggle; i < sent.size(); ++i) {
        if (sent[i].code == guiCode && sent[i].action == KEY_ACTION_UP &&
            sent[i].modifiers == MODIFIER_EXTENDED && sent[i].flags == 0) {
            guiReleasedWithFlags = true;
        }
    }
    check(guiReleasedWithFlags, context + " synthesized GUI up preserves extended flag");
    releaseModifiers(handler, guiMod);
    emitKey(handler, gui, symbol, false, KMOD_NONE);
    check(hostKeys.empty() && handler.m_KeysDown.isEmpty(),
          context + " physical key-ups leave no stuck host key");
}
int main() {
    for (auto mode : {StreamingPreferences::CSK_ALWAYS,
                      StreamingPreferences::CSK_FULLSCREEN}) {
        disableWithHeldGui(SDL_SCANCODE_LGUI, SDLK_LGUI, KMOD_LGUI, mode, false);
        disableWithHeldGui(SDL_SCANCODE_RGUI, SDLK_RGUI, KMOD_RGUI, mode, true);
    }
    {
        SdlInputHandler handler(StreamingPreferences::CSK_OFF, true);
        pressModifiers(handler, 0);
        emitKey(handler, SDL_SCANCODE_K, SDLK_k, true, shortcutMods);
        check(handler.m_CaptureSystemKeysMode == StreamingPreferences::CSK_ALWAYS &&
              keyboardGrab && relativeMouse && handler.m_KeysDown.size() == 3,
              "enabling capture preserves already forwarded modifiers");
        releaseModifiers(handler, 0);
        check(hostKeys.empty(), "physical modifier releases after enabling reach host");
    }
    {
        SdlInputHandler handler(StreamingPreferences::CSK_OFF, false);
        emitKey(handler, SDL_SCANCODE_K, SDLK_k, true, shortcutMods);
        check(handler.m_CaptureSystemKeysMode == StreamingPreferences::CSK_ALWAYS &&
              !keyboardGrab && !relativeMouse,
              "K does not reactivate released mouse capture");
    }
    {
        SdlInputHandler handler(StreamingPreferences::CSK_ALWAYS, true);
        emitKey(handler, SDL_SCANCODE_LGUI, SDLK_LGUI, true, KMOD_LGUI);
        pressModifiers(handler, KMOD_LGUI);
        emitKey(handler, SDL_SCANCODE_Z, SDLK_z, true, shortcutMods | KMOD_LGUI);
        check(!relativeMouse && !keyboardGrab && hostKeys.empty() &&
              handler.m_KeysDown.isEmpty(), "Z still releases mouse, keyboard, and keys");
    }
    {
        SdlInputHandler handler(StreamingPreferences::CSK_ALWAYS, true);
        emitKey(handler, SDL_SCANCODE_A, SDLK_a, true, KMOD_NONE);
        pressModifiers(handler, 0);
        const size_t beforeToggle = sent.size();
        emitKey(handler, SDL_SCANCODE_K, SDLK_k, true, shortcutMods);
        check(hostKeys.empty() && handler.m_KeysDown.isEmpty() &&
              sent.size() == beforeToggle + 4,
              "K releases held ordinary key plus Ctrl/Alt/Shift");
        const size_t afterToggle = sent.size();
        emitKey(handler, SDL_SCANCODE_A, SDLK_a, true, KMOD_NONE, true);
        check(sent.size() == afterToggle, "held ordinary key repeat stays ignored");
        emitKey(handler, SDL_SCANCODE_A, SDLK_a, false, KMOD_NONE);
        emitKey(handler, SDL_SCANCODE_K, SDLK_k, false, shortcutMods);
        releaseModifiers(handler, 0);
        check(hostKeys.empty() && handler.m_KeysDown.isEmpty(),
              "duplicate physical releases after K cannot restore down state");
        emitKey(handler, SDL_SCANCODE_A, SDLK_a, true, KMOD_NONE);
        check(hostKeys.count(HostKey(short(0x8041), 0, 0)) == 1 &&
              handler.m_KeysDown.size() == 1,
              "ordinary key re-press still forwards while capture is OFF");
        emitKey(handler, SDL_SCANCODE_A, SDLK_a, false, KMOD_NONE);
        check(hostKeys.empty() && handler.m_KeysDown.isEmpty(),
              "ordinary key after OFF has matching release");
    }
    {
        SdlInputHandler handler(StreamingPreferences::CSK_ALWAYS, true);
        struct HeldKey { SDL_Scancode scan; short code; int extended, flags; };
        const HeldKey keys[] = {
            {SDL_SCANCODE_RETURN, short(0x800D), 0, 0},
            {SDL_SCANCODE_KP_ENTER, short(0x800D), MODIFIER_EXTENDED, 0},
            {SDL_SCANCODE_RCTRL, short(0x80A3), MODIFIER_EXTENDED, 0},
            {SDL_SCANCODE_RALT, short(0x80A5), MODIFIER_EXTENDED, 0},
            {SDL_SCANCODE_RSHIFT, short(0x80A1), 0, 0},
            {SDL_SCANCODE_BACKSLASH, short(0x80DC), 0, 0},
            {SDL_SCANCODE_INTERNATIONAL3, short(0x80DC), 0, SS_KBE_FLAG_NON_NORMALIZED},
            {SDL_SCANCODE_NONUSBACKSLASH, short(0x80E2), 0, 0},
            {SDL_SCANCODE_INTERNATIONAL1, short(0x80E2), 0, SS_KBE_FLAG_NON_NORMALIZED},
        };
        std::multiset<std::tuple<short, int, int>> expectedReleases;
        for (const auto& key : keys) {
            emitKey(handler, key.scan, SDLK_UNKNOWN, true, KMOD_NONE);
            expectedReleases.emplace(key.code, key.extended, key.flags);
        }
        pressModifiers(handler, 0);
        expectedReleases.emplace(short(0x80A2), 0, 0);
        expectedReleases.emplace(short(0x80A4), 0, 0);
        expectedReleases.emplace(short(0x80A0), 0, 0);
        check(handler.m_KeysDown.size() == 12 && hostKeys.size() == 12,
              "tracking distinguishes shared VK codes by extended and normalized flags");
        const size_t beforeToggle = sent.size();
        emitKey(handler, SDL_SCANCODE_K, SDLK_k, true, shortcutMods);
        std::multiset<std::tuple<short, int, int>> actualReleases;
        for (size_t i = beforeToggle; i < sent.size(); ++i) {
            check(sent[i].action == KEY_ACTION_UP, "K cleanup sends only key-ups");
            actualReleases.emplace(sent[i].code, sent[i].modifiers, sent[i].flags);
        }
        check(actualReleases == expectedReleases,
              "K restores all extended and nonnormalized flags without modifier mask");
        check(hostKeys.empty() && handler.m_KeysDown.isEmpty(),
              "mixed flagged keys leave no tracked or host state after OFF");
    }
    {
        SdlInputHandler handler(StreamingPreferences::CSK_ALWAYS, true);
        for (auto scan : {SDL_SCANCODE_A, SDL_SCANCODE_RCTRL,
                          SDL_SCANCODE_INTERNATIONAL3}) {
            emitKey(handler, scan, SDLK_UNKNOWN, true, shortcutMods | KMOD_GUI);
            emitKey(handler, scan, SDLK_UNKNOWN, false, KMOD_NONE);
            check(handler.m_KeysDown.isEmpty(),
                  "changed Ctrl/Alt/Shift/Meta state does not block key-up bookkeeping");
        }
        const size_t beforeCleanup = sent.size();
        handler.raiseAllKeys();
        check(hostKeys.empty() && sent.size() == beforeCleanup,
              "empty cleanup does not send an extra release");
    }
    {
        SdlInputHandler handler(StreamingPreferences::CSK_ALWAYS, true);
        emitKey(handler, SDL_SCANCODE_LGUI, SDLK_LGUI, true, KMOD_LGUI);
        pressModifiers(handler, KMOD_LGUI);
        emitKey(handler, SDL_SCANCODE_K, SDLK_k, true, shortcutMods | KMOD_LGUI);
        const size_t afterToggle = sent.size();
        windowFlags &= ~SDL_WINDOW_INPUT_FOCUS;
        handler.notifyFocusLost();
        check(hostKeys.empty() && handler.m_KeysDown.isEmpty() &&
              sent.size() == afterToggle,
              "focus loss after K OFF does not release keys twice");
    }
    {
        SdlInputHandler handler(StreamingPreferences::CSK_FULLSCREEN, true);
        windowFlags &= ~SDL_WINDOW_FULLSCREEN;
        handler.updateKeyboardGrabState();
        check(!keyboardGrab && !handler.isSystemKeyCaptureActive(),
              "FULLSCREEN mode is inactive in windowed mode");
        pressModifiers(handler, 0);
        emitKey(handler, SDL_SCANCODE_K, SDLK_k, true, shortcutMods);
        check(handler.m_CaptureSystemKeysMode == StreamingPreferences::CSK_ALWAYS &&
              keyboardGrab && handler.m_KeysDown.size() == 3,
              "K enables windowed inactive FULLSCREEN mode without raising keys");
        releaseModifiers(handler, 0);
        check(hostKeys.empty(), "windowed enable preserves matching physical key-ups");
    }
    if (failures) std::cerr << failures << " failed checks\n";
    else std::cout << "PASS 12 scenarios: GUI, ordinary keys, on/off paths, "
                        "extended/nonnormalized flags, bookkeeping, repeats, focus cleanup\n";
    return failures ? 1 : 0;
}
'''


def run_variant(label: str, keyboard: str, input_source: str, input_header: str,
                build_dir: Path, compiler: str, source_root: Path,
                qt_cflags: list[str], qt_libs: list[str]) -> subprocess.CompletedProcess:
    source = build_dir / f"{label}.cpp"
    binary = build_dir / label
    source.write_text(harness(keyboard, input_source, input_header))
    subprocess.run([compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror",
                    *qt_cflags, "-I", str(source_root / "app"), "-I",
                    str(source_root / "moonlight-common-c/moonlight-common-c/src"),
                    str(source), "-o", str(binary), *qt_libs], check=True)
    result = subprocess.run([str(binary)], capture_output=True, text=True)
    print(f"{label} (exit {result.returncode}):")
    print(result.stdout + result.stderr, end="")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path,
                        default=Path(__file__).resolve().parents[1])
    parser.add_argument("--baseline-ref", default="v6.2.0")
    parser.add_argument("--compiler", default="c++")
    args = parser.parse_args()
    source_root = args.source_root.resolve()
    reference = subprocess.run(
        ["git", "cat-file", "-e", f"{args.baseline_ref}:app/streaming/input/keyboard.cpp"],
        cwd=source_root, capture_output=True, text=True,
    )
    if reference.returncode:
        parser.error("Baseline source is unavailable. Fetch the v6.2.0 tag or provide --baseline-ref.")
    qt_cflags = shlex.split(subprocess.run(
        ["pkg-config", "--cflags", "Qt6Core", "Qt6Qml", "sdl2"],
        capture_output=True, text=True, check=True,
    ).stdout)
    qt_libs = shlex.split(subprocess.run(
        ["pkg-config", "--libs", "Qt6Core"],
        capture_output=True, text=True, check=True,
    ).stdout)
    baseline = []
    for name in ("keyboard.cpp", "input.cpp", "input.h"):
        baseline.append(subprocess.run(
            ["git", "show", f"{args.baseline_ref}:app/streaming/input/{name}"],
            cwd=source_root, capture_output=True, text=True, check=True,
        ).stdout)
    working = [(source_root / "app/streaming/input" / name).read_text()
               for name in ("keyboard.cpp", "input.cpp", "input.h")]
    with tempfile.TemporaryDirectory(prefix="moonlight-key-toggle-") as directory:
        build_dir = Path(directory)
        before = run_variant("baseline", *baseline, build_dir, args.compiler,
                             source_root, qt_cflags, qt_libs)
        after = run_variant("working", *working, build_dir, args.compiler,
                            source_root, qt_cflags, qt_libs)
    if before.returncode != 1 or any(before.stderr.count(message) != 2 for message in (
        "FAIL LGUI physical key-ups leave no stuck host key",
        "FAIL RGUI physical key-ups leave no stuck host key",
    )):
        print("Expected the tagged baseline to reproduce all four GUI-key failures.")
        return 1
    if after.returncode:
        print("Working source still fails the regression checks.")
        return 1
    print("PASS baseline reproduces the regression; working source fixes it.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
