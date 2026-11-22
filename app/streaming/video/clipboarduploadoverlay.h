#pragma once

#include "../clipboarduploadprogress.h"
#include "SDL_compat.h"
#include <QCoreApplication>
#include <QImage>
#include <QLocale>
#include <QPainter>
#include <QReadWriteLock>
#include <QSet>
#include <chrono>
#include <deque>

// The stream loop owns hit testing. The decoder can request a fresh surface on another thread.
class ClipboardUploadOverlay
{
public:
    enum class Input { Forward, Handled, Cancel };

    bool update(const ClipboardUploadProgress& upload, QSize window, qreal scale)
    {
        QWriteLocker lock(&m_Lock);
        if (upload.startedAtMs != m_Upload.startedAtMs || upload.resumedBytes != m_Upload.resumedBytes ||
                upload.names.isEmpty() || upload.preparing || upload.committing) {
            m_Samples.clear();
        }
        const qint64 rate = upload.names.isEmpty() || upload.preparing || upload.committing ? -1 : sampleRate(upload);
        if (window == m_Window && m_Scale == scale && upload.names == m_Upload.names && upload.index == m_Upload.index &&
                upload.acknowledged == m_Upload.acknowledged && upload.committing == m_Upload.committing &&
                upload.preparing == m_Upload.preparing && upload.resumedBytes == m_Upload.resumedBytes &&
                upload.startedAtMs == m_Upload.startedAtMs && rate == m_BytesPerSecond) return false;
        m_BytesPerSecond = rate;
        m_Window = window;
        m_Scale = scale;
        m_Layout = window / scale;
        m_Upload = upload;
        if (upload.names.isEmpty()) m_Hover = false;
        return true;
    }

    SDL_Rect destinationRect(int width, int height) const
    {
        QReadLocker lock(&m_Lock);
        const auto size = panelSize(m_Layout);
        const auto rect = panelRect(m_Layout.width(), m_Layout.height(), size.width(), size.height());
        const qreal sx = qreal(width) / qMax(1, m_Layout.width());
        const qreal sy = qreal(height) / qMax(1, m_Layout.height());
        return {qRound(rect.x * sx), qRound(rect.y * sy), qRound(rect.w * sx), qRound(rect.h * sy)};
    }

    static SDL_Rect panelRect(int width, int height, int w, int h)
    {
        return {qMax(0, width - w - Margin), qMax(0, height - h - Margin), w, h};
    }

    // Captured releases must be consumed even after the transfer is cancelled or completes.
    Input handle(const SDL_Event& event, bool& hoverChanged)
    {
        hoverChanged = false;
        // update() and handle() run on the same thread. The renderer only reads.
        if (m_Upload.names.isEmpty() && !m_Buttons && m_Fingers.isEmpty() && !m_Escape) return Input::Forward;
        QWriteLocker lock(&m_Lock);
        const QSize size = panelSize(m_Layout);
        const auto panel = panelRect(m_Layout.width(), m_Layout.height(), size.width(), size.height());
        const QRect bounds(panel.x, panel.y, panel.w, panel.h);
        const auto cancel = cancelRect(panel.w).translated(bounds.topLeft());
        const bool visible = !m_Upload.names.isEmpty();
        auto mousePoint = [&](int x, int y) { return QPoint(int(x / m_Scale), int(y / m_Scale)); };
        switch (event.type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            if (event.key.keysym.scancode != SDL_SCANCODE_ESCAPE) break;
            if (event.type == SDL_KEYUP) {
                if (!m_Escape) break;
                m_Escape = false;
                return Input::Handled;
            }
            if (m_Escape && event.key.repeat) return Input::Handled;
            m_Escape = visible && !event.key.repeat;
            return m_Escape ? Input::Cancel : Input::Forward;
        case SDL_MOUSEMOTION: {
            const auto p = mousePoint(event.motion.x, event.motion.y);
            const bool hover = visible && cancel.contains(p);
            hoverChanged = hover != m_Hover;
            m_Hover = hover;
            if (m_Buttons || (visible && bounds.contains(p) && !event.motion.state)) return Input::Handled;
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            if (!event.button.button || event.button.button > 32) break;
            const Uint32 button = Uint32(1) << (event.button.button - 1);
            if (event.type == SDL_MOUSEBUTTONUP) {
                if (!(m_Buttons & button)) break;
                m_Buttons &= ~button;
                return Input::Handled;
            }
            m_Buttons &= ~button;
            const auto p = mousePoint(event.button.x, event.button.y);
            if (!visible || !bounds.contains(p)) break;
            if (event.button.which == SDL_TOUCH_MOUSEID) return Input::Handled;
            m_Buttons |= button;
            return event.button.button == SDL_BUTTON_LEFT && cancel.contains(p) ? Input::Cancel : Input::Handled;
        }
        case SDL_FINGERDOWN:
        case SDL_FINGERUP:
        case SDL_FINGERMOTION: {
            const auto id = qMakePair(event.tfinger.touchId, event.tfinger.fingerId);
            if (event.type == SDL_FINGERUP) return m_Fingers.remove(id) ? Input::Handled : Input::Forward;
            if (event.type == SDL_FINGERMOTION) return m_Fingers.contains(id) ? Input::Handled : Input::Forward;
            if (!visible || SDL_GetTouchDeviceType(event.tfinger.touchId) != SDL_TOUCH_DEVICE_DIRECT) break;
            const QPoint p(int(event.tfinger.x * m_Layout.width()), int(event.tfinger.y * m_Layout.height()));
            if (!bounds.contains(p)) break;
            m_Fingers.insert(id);
            return cancel.contains(p) ? Input::Cancel : Input::Handled;
        }
        case SDL_MOUSEWHEEL: {
            int x, y;
            SDL_GetMouseState(&x, &y);
            if (visible && bounds.contains(mousePoint(x, y))) return Input::Handled;
            break;
        }
        }
        return Input::Forward;
    }

    SDL_Surface* render() const
    {
        QReadLocker lock(&m_Lock);
        if (m_Upload.names.isEmpty()) return nullptr;
        const auto size = panelSize(m_Layout);
        const auto pixels = size * m_Scale;
        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, pixels.width(), pixels.height(), 32, SDL_PIXELFORMAT_ARGB8888);
        if (!surface) return nullptr;
        QImage image(static_cast<uchar*>(surface->pixels), pixels.width(), pixels.height(), surface->pitch, QImage::Format_ARGB32);
        image.fill(Qt::transparent);
        QPainter p(&image);
        p.scale(m_Scale, m_Scale);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QColor("#506078"));
        p.setBrush(QColor(22, 30, 44, 248));
        p.drawRoundedRect(QRect(QPoint(), size).adjusted(1, 1, -1, -1), 10, 10);
        const int w = size.width();
        const auto button = cancelRect(w);
        auto text = [&](QRect rect, QString value, int pixels, const char* color, bool bold = false, bool right = false) {
            QFont font = p.font();
            font.setPixelSize(pixels);
            font.setBold(bold);
            p.setFont(font);
            p.setPen(QColor(color));
            p.drawText(rect, (right ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter,
                       p.fontMetrics().elidedText(value, Qt::ElideMiddle, qMax(0, rect.width())));
        };
        const auto tr = [](const char* source) { return QCoreApplication::translate("ClipboardTransfer", source); };
        QString title;
        if (m_Upload.preparing) title = tr("Preparing files (%1)").arg(m_Upload.names.size());
        else if (m_Upload.committing) title = tr("Waiting for host");
        else title = (m_Upload.resumedBytes ? tr("Resuming %1 of %2") : tr("Uploading %1 of %2"))
                             .arg(m_Upload.index + 1).arg(m_Upload.names.size());
        text(QRect(14, 7, button.x() - 19, 26), title, 14, "#f2f5fa", true);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(m_Hover ? "#566a87" : "#344258"));
        p.drawRoundedRect(button, 5, 5);
        QFont font = p.font();
        font.setPixelSize(12);
        p.setFont(font);
        p.setPen(Qt::white);
        p.drawText(button, Qt::AlignCenter, tr("Cancel (Esc)"));
        int y = 39;
        for (int row = 0; row < rows(m_Layout); ++row) {
            const bool current = row == 0;
            const int percent = m_Upload.fileSize ? int(m_Upload.fileBytes * 100 / m_Upload.fileSize) : 100;
            text(QRect(14, y, w - 96, 20), m_Upload.names.at(m_Upload.index + row), 13,
                 current ? "#e3ebf6" : "#a6b3c6", current);
            text(QRect(w - 79, y, 65, 20), current ? QString::number(percent) + '%' : tr("Queued"),
                 12, current ? "#91c2ff" : "#a6b3c6", false, true);
            y += 21;
        }
        const int track = qMax(1, w - 72);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#42516a"));
        p.drawRoundedRect(QRect(14, y + 6, track, 4), 2, 2);
        p.setBrush(QColor("#81b7ff"));
        p.drawRoundedRect(QRect(14, y + 6, track * m_Upload.percent() / 100, 4), 2, 2);
        text(QRect(w - 54, y, 40, 16), QString::number(m_Upload.percent()) + '%', 12, "#e3ebf6", true, true);
        const QLocale locale;
        const bool stacked = w < 270;
        const int available = qMax(0, w - 28);
        const int speedWidth = stacked ? available : available * 45 / 100;
        text(QRect(14, y + 17, stacked ? available : available - speedWidth - 4, 17),
             QStringLiteral("%1 / %2").arg(locale.formattedDataSize(m_Upload.acknowledged),
                                            locale.formattedDataSize(m_Upload.total)), 12, "#c6d1e0");
        const QString speed = m_BytesPerSecond < 0 ? tr("Speed —") :
                              tr("Speed %1/s").arg(locale.formattedDataSize(m_BytesPerSecond, 1));
        text(QRect(w - 14 - speedWidth, y + (stacked ? 34 : 17), speedWidth, 17),
             speed, 12, "#c6d1e0", false, !stacked);
        return surface;
    }

private:
    struct RateSample {
        qint64 timeMs;
        quint64 bytes;
    };

    qint64 sampleRate(const ClipboardUploadProgress& upload)
    {
        using namespace std::chrono;
        const qint64 now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
        const quint64 bytes = upload.acknowledged - qMin(upload.acknowledged, upload.resumedBytes);
        if (m_Samples.empty() || bytes < m_Samples.back().bytes) {
            m_Samples = {{now, bytes}};
            return -1;
        }
        if (now - m_Samples.back().timeMs < 250) return m_BytesPerSecond;
        // Include a stationary sample even when no ACK arrives so a stalled transfer falls to zero.
        while (m_Samples.size() > 1 && now - m_Samples[1].timeMs >= 2000) m_Samples.pop_front();
        m_Samples.push_back({now, bytes});
        const auto& oldest = m_Samples.front();
        const auto elapsed = now - oldest.timeMs;
        return elapsed < 500 ? -1 : qint64((bytes - oldest.bytes) * 1000 / elapsed);
    }

    static constexpr int Margin = 12;
    static QRect cancelRect(int w) { return QRect(qMax(0, w - 100), 6, 86, 28); }
    int rows(QSize window) const
    {
        return qMin(qBound(1, (window.height() - 95) / 21, 3), int(m_Upload.names.size()) - m_Upload.index);
    }
    QSize panelSize(QSize window) const
    {
        const int width = qBound(1, window.width() - Margin * 2, 332);
        return QSize(width, 80 + 21 * rows(window) + (width < 270 ? 17 : 0));
    }

    mutable QReadWriteLock m_Lock;
    ClipboardUploadProgress m_Upload;
    std::deque<RateSample> m_Samples;
    qint64 m_BytesPerSecond = -1;
    QSize m_Window;
    QSize m_Layout;
    qreal m_Scale = 1;
    bool m_Hover = false;
    bool m_Escape = false;
    Uint32 m_Buttons = 0;
    QSet<QPair<SDL_TouchID, SDL_FingerID>> m_Fingers;
};
