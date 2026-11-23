#include "../streaming/video/clipboarduploadoverlay.h"
#include <cassert>

int main()
{
    using Input = ClipboardUploadOverlay::Input;
    ClipboardUploadOverlay overlay;
    ClipboardUploadProgress transfer;
    transfer.names = {QStringLiteral("example.bin")};
    transfer.taskId = 7;
    transfer.total = transfer.fileSize = 200;
    const QSize window(1200, 900);
    const qreal scale = 1.5;
    overlay.update(transfer, window, scale);
    auto rect = [&] { return overlay.destinationRect(window.width(), window.height()); };
    auto button = [&](Uint32 type, QPoint point) {
        SDL_Event e{};
        e.type = type;
        e.button.button = SDL_BUTTON_LEFT;
        e.button.x = point.x();
        e.button.y = point.y();
        bool changed;
        return overlay.handle(e, changed);
    };
    const auto original = rect();
    const QPoint grab(original.x + 30, original.y + 20);
    assert(button(SDL_MOUSEBUTTONDOWN, grab) == Input::Handled);
    SDL_Event move{};
    move.type = SDL_MOUSEMOTION;
    move.motion.state = SDL_BUTTON_LMASK;
    move.motion.x = 150;
    move.motion.y = 90;
    bool changed = false;
    assert(overlay.handle(move, changed) == Input::Handled && changed);
    assert(button(SDL_MOUSEBUTTONUP, {150, 90}) == Input::Handled);
    const auto moved = rect();
    assert(moved.x == 120 && qAbs(moved.y - 70) <= 2);
    auto toggle = [&] {
        const auto r = rect();
        QPoint center(r.x + r.w - qRound(26 * scale), r.y + qRound(20 * scale));
        assert(button(SDL_MOUSEBUTTONDOWN, center) == Input::Handled);
        assert(button(SDL_MOUSEBUTTONUP, center) == Input::Handled);
    };
    toggle();
    const auto compact = rect();
    assert(compact.w < moved.w && compact.h < moved.h);
    transfer.fileBytes = transfer.acknowledged = 150;
    overlay.update(transfer, window, scale);
    assert(rect().x == compact.x && rect().y == compact.y && rect().h == compact.h);
    assert(button(SDL_MOUSEBUTTONDOWN, {compact.x + 20, compact.y + compact.h + 10}) == Input::Forward);
    toggle();
    assert(rect().h == moved.h && rect().w == moved.w);
    assert(button(SDL_MOUSEBUTTONDOWN, {moved.x + moved.w - qRound(89 * scale), moved.y + 25}) == Input::Cancel);
}
