#pragma once

#include "../clipboarduploadprogress.h"
#include "../transferratesampler.h"
#include "SDL_compat.h"
#include <QCoreApplication>
#include <QImage>
#include <QLocale>
#include <QPainter>
#include <QReadWriteLock>
#include <QSet>
#include <algorithm>
#include <optional>

// The stream loop owns hit testing. The decoder can request a fresh surface on another thread.
class ClipboardUploadOverlay
{
public:
    enum class Input { Forward, Handled, Cancel, Paste };
    enum class Header { None, Drag, Toggle, Cancel };

    bool update(const ClipboardUploadProgress& upload, QSize window, qreal scale)
    {
        return updateTasks(upload.names.isEmpty() ? QList<ClipboardUploadProgress>() : QList<ClipboardUploadProgress>{upload}, window, scale);
    }

    bool updateTasks(QList<ClipboardUploadProgress> tasks, QSize window, qreal scale)
    {
        using State = ClipboardUploadProgress::State;
        QWriteLocker lock(&m_Lock);
        // Keep the active data transfer visible even when an older job needs attention.
        auto active = std::find_if(tasks.begin(), tasks.end(), [](const auto& job) { return job.state == State::Uploading; });
        const auto upload = active != tasks.end() ? *active : tasks.value(0);
        if (active != tasks.end()) tasks.erase(active);
        else if (!tasks.isEmpty()) tasks.removeFirst();
        const bool measuring = upload.state == State::Uploading && !upload.names.isEmpty() && !upload.preparing && !upload.committing;
        if (!measuring || upload.taskId != m_Upload.taskId || upload.startedAtMs != m_Upload.startedAtMs ||
            upload.resumedBytes != m_Upload.resumedBytes) m_Rate.reset();
        const auto newBytes = upload.acknowledged - qMin(upload.acknowledged, upload.resumedBytes);
        const qint64 rate = measuring ? m_Rate.sample(newBytes) : -1;
        if (window == m_Window && m_Scale == scale && upload == m_Upload && tasks == m_Others && rate == m_BytesPerSecond) return false;
        m_BytesPerSecond = rate;
        m_Window = window;
        m_Scale = scale;
        m_Layout = window / scale;
        m_Upload = upload;
        m_Others = std::move(tasks);
        m_First = qBound(0, m_First, qMax(0, int(m_Others.size()) - otherRows()));
        if (!hasTask()) {
            m_Hover = Header::None;
            m_MouseDrag = false;
            m_DragFinger.reset();
        }
        return true;
    }

    SDL_Rect destinationRect(int width, int height) const
    {
        QReadLocker lock(&m_Lock);
        const auto size = panelSize(m_Layout);
        const auto rect = position(size);
        const qreal sx = qreal(width) / qMax(1, m_Layout.width());
        const qreal sy = qreal(height) / qMax(1, m_Layout.height());
        return {qRound(rect.x * sx), qRound(rect.y * sy), qRound(rect.w * sx), qRound(rect.h * sy)};
    }

    static SDL_Rect panelRect(int width, int height, int w, int h)
    {
        return {qMax(0, width - w - Margin), qMax(0, height - h - Margin), w, h};
    }

    // Captured releases must be consumed even after the transfer is cancelled or completes.
    Input handle(const SDL_Event& event, bool& hoverChanged, quint64* selected = nullptr)
    {
        hoverChanged = false;
        // update() and handle() run on the same thread. The renderer only reads.
        if (!hasTask() && !m_Buttons && m_Fingers.isEmpty() && !m_Escape) return Input::Forward;
        QWriteLocker lock(&m_Lock);
        const QSize size = panelSize(m_Layout);
        const auto panel = position(size);
        const QRect bounds(panel.x, panel.y, panel.w, panel.h);
        const bool visible = hasTask();
        auto mousePoint = [&](int x, int y) { return QPoint(int(x / m_Scale), int(y / m_Scale)); };
        auto hit = [&](QPoint point) {
            const auto local = point - bounds.topLeft();
            if (selected) *selected = m_Upload.taskId;
            const auto header = headerAt(local, panel.w);
            if (header == Header::Toggle) {
                m_Collapsed = !m_Collapsed;
                m_Hover = Header::None;
                hoverChanged = true;
                return Input::Handled;
            }
            if (header == Header::Cancel) return Input::Cancel;
            if (m_Collapsed) return Input::Handled;
            if (m_Upload.state == ClipboardUploadProgress::State::NeedsPaste && pasteRect(panel.w).contains(local)) return Input::Paste;
            const int y = local.y() - summaryHeight(m_Layout);
            if (y < 0) return Input::Handled;
            const int row = y / Row;
            if (row >= otherRows()) {
                if (m_Others.size() > otherRows()) {
                    m_First = m_First + otherRows() >= m_Others.size() ? 0 : qMin(m_First + otherRows(), int(m_Others.size()) - otherRows());
                    hoverChanged = true;
                }
                return Input::Handled;
            }
            const auto& job = m_Others.at(m_First + row);
            if (selected) *selected = job.taskId;
            if (rowCancel(panel.w, row).contains(local)) return Input::Cancel;
            if (job.state == ClipboardUploadProgress::State::NeedsPaste && rowPaste(panel.w, row).contains(local)) return Input::Paste;
            return Input::Handled;
        };
        auto drag = [&](QPoint point) {
            m_Position = point - m_Grab;
            hoverChanged = true;
            return Input::Handled;
        };
        switch (event.type) {
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                m_MouseDrag = false;
                m_DragFinger.reset();
            }
            break;
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
            if (selected) *selected = m_Upload.taskId;
            return m_Escape ? Input::Cancel : Input::Forward;
        case SDL_MOUSEMOTION: {
            const auto p = mousePoint(event.motion.x, event.motion.y);
            if (m_MouseDrag && visible && (event.motion.state & SDL_BUTTON_LMASK)) return drag(p);
            const auto hover = visible ? headerAt(p - bounds.topLeft(), panel.w) : Header::None;
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
                if (event.button.button == SDL_BUTTON_LEFT) m_MouseDrag = false;
                return Input::Handled;
            }
            m_Buttons &= ~button;
            const auto p = mousePoint(event.button.x, event.button.y);
            if (!visible || !bounds.contains(p)) break;
            if (event.button.which == SDL_TOUCH_MOUSEID) return Input::Handled;
            m_Buttons |= button;
            if (event.button.button == SDL_BUTTON_LEFT && headerAt(p - bounds.topLeft(), panel.w) == Header::Drag && !m_DragFinger) {
                m_Grab = p - bounds.topLeft();
                m_MouseDrag = true;
            }
            return event.button.button == SDL_BUTTON_LEFT ? hit(p) : Input::Handled;
        }
        case SDL_FINGERDOWN:
        case SDL_FINGERUP:
        case SDL_FINGERMOTION: {
            const auto id = qMakePair(event.tfinger.touchId, event.tfinger.fingerId);
            if (event.type == SDL_FINGERUP) {
                if (m_DragFinger == id) m_DragFinger.reset();
                return m_Fingers.remove(id) ? Input::Handled : Input::Forward;
            }
            const QPoint p(int(event.tfinger.x * m_Layout.width()), int(event.tfinger.y * m_Layout.height()));
            if (event.type == SDL_FINGERMOTION) {
                if (visible && m_DragFinger == id) return drag(p);
                return m_Fingers.contains(id) ? Input::Handled : Input::Forward;
            }
            if (!visible || SDL_GetTouchDeviceType(event.tfinger.touchId) != SDL_TOUCH_DEVICE_DIRECT) break;
            if (!bounds.contains(p)) break;
            if (headerAt(p - bounds.topLeft(), panel.w) == Header::Drag && !m_DragFinger && !m_MouseDrag) {
                m_Grab = p - bounds.topLeft();
                m_DragFinger = id;
            }
            m_Fingers.insert(id);
            return hit(p);
        }
        case SDL_MOUSEWHEEL: {
            int x, y;
            SDL_GetMouseState(&x, &y);
            if (visible && bounds.contains(mousePoint(x, y))) {
                m_First = qBound(0, m_First - event.wheel.y, qMax(0, int(m_Others.size()) - otherRows()));
                hoverChanged = true;
                return Input::Handled;
            }
            break;
        }
        }
        return Input::Forward;
    }

    SDL_Surface* render() const
    {
        QReadLocker lock(&m_Lock);
        if (!hasTask()) return nullptr;
        const auto size = panelSize(m_Layout);
        const auto pixels = size * m_Scale;
        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, pixels.width(), pixels.height(), 32, SDL_PIXELFORMAT_ARGB8888);
        if (!surface) return nullptr;
        QImage image(static_cast<uchar*>(surface->pixels), pixels.width(), pixels.height(), surface->pitch, QImage::Format_ARGB32);
        paint(image, size, false);
        return surface;
    }

    QImage desktopImage() const
    {
        QReadLocker lock(&m_Lock);
        const auto size = panelSize(m_Layout);
        QImage image(size * m_Scale, QImage::Format_ARGB32_Premultiplied);
        if (!image.isNull()) paint(image, size, true);
        return image;
    }

    Header desktopHeader(QPoint point) const
    {
        QReadLocker lock(&m_Lock);
        return headerAt(point, panelSize(m_Layout).width());
    }

    void toggleCollapsed()
    {
        QWriteLocker lock(&m_Lock);
        m_Collapsed = !m_Collapsed;
        m_Hover = Header::None;
    }

    bool setHeaderHovered(Header hovered)
    {
        QWriteLocker lock(&m_Lock);
        if (m_Hover == hovered) return false;
        m_Hover = hovered;
        return true;
    }

private:
    void paint(QImage& image, QSize size, bool desktop) const
    {
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
        using State = ClipboardUploadProgress::State;
        if (m_Upload.state != State::Uploading) title = status(m_Upload);
        else if (m_Upload.downloading && m_Upload.names.isEmpty()) title = tr("Receiving files");
        else if (m_Upload.downloading) title = m_Upload.committing ? tr("Preparing local clipboard") :
                                            tr("Downloading %1 of %2").arg(m_Upload.index + 1).arg(m_Upload.names.size());
        else if (m_Upload.preparing) title = tr("Preparing files (%1)").arg(m_Upload.names.size());
        else if (m_Upload.committing) title = tr("Waiting for host");
        else title = (m_Upload.resumedBytes ? tr("Resuming %1 of %2") : tr("Uploading %1 of %2"))
                             .arg(m_Upload.index + 1).arg(m_Upload.names.size());
        const auto toggle = toggleRect(w);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(m_Hover == Header::Toggle ? "#566a87" : "#344258"));
        p.drawRoundedRect(toggle, 5, 5);
        const auto center = toggle.center();
        p.setPen(QPen(QColor("#e3ebf6"), 1.5));
        p.setBrush(Qt::NoBrush);
        if (m_Collapsed) {
            p.drawRect(QRect(center.x() - 5, center.y() - 4, 10, 8));
            const int percent = m_Upload.percent();
            const bool showPercent = m_Upload.state == State::Uploading && percent >= 0;
            const int end = toggle.left() - 6;
            text(QRect(12, 6, end - 12 - (showPercent ? 43 : 0), 28), title, 13, "#f2f5fa", true);
            if (showPercent) text(QRect(end - 42, 6, 42, 28), QString::number(percent) + '%', 13, "#91c2ff", true, true);
            return;
        }
        p.drawLine(center + QPoint(-5, 2), center + QPoint(5, 2));
        text(QRect(14, 7, (m_Upload.state == State::NeedsPaste ? pasteRect(w).x() : button.x()) - 19, 26), title, 14, "#f2f5fa", true);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(m_Hover == Header::Cancel ? "#566a87" : "#344258"));
        p.drawRoundedRect(button, 5, 5);
        QFont font = p.font();
        font.setPixelSize(12);
        p.setFont(font);
        p.setPen(Qt::white);
        p.drawText(button, Qt::AlignCenter, finished(m_Upload) ? tr("Dismiss") : desktop ? tr("Cancel") : tr("Cancel (Esc)"));
        auto action = [&](QRect rect, const QString& label) {
            p.setPen(Qt::NoPen);
            p.setBrush(QColor("#344258"));
            p.drawRoundedRect(rect, 4, 4);
            p.setFont(font);
            p.setPen(Qt::white);
            p.drawText(rect, Qt::AlignCenter, label);
        };
        if (m_Upload.state == State::NeedsPaste) action(pasteRect(w), tr("Paste here"));
        int y = 39;
        for (int row = 0; row < rows(m_Layout); ++row) {
            const bool current = row == 0;
            const int percent = m_Upload.preparing ? 0 : ClipboardUploadProgress::percentOf(m_Upload.fileBytes, m_Upload.fileSize);
            text(QRect(14, y, w - 96, 20), m_Upload.names.at(m_Upload.index + row), 13,
                 current ? "#e3ebf6" : "#a6b3c6", current);
            text(QRect(w - 79, y, 65, 20), m_Upload.state != State::Uploading ? QString() : current ? QString::number(percent) + '%' : tr("Queued"),
                 12, current ? "#91c2ff" : "#a6b3c6", false, true);
            y += 21;
        }
        if (m_Upload.state == State::Uploading && !m_Upload.names.isEmpty()) {
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
        }
        for (int i = 0; i < otherRows(); ++i) {
            const auto& job = m_Others.at(m_First + i);
            const int top = summaryHeight(m_Layout) + i * Row;
            const auto cancel = rowCancel(w, i);
            const bool needsPaste = job.state == State::NeedsPaste;
            const int end = (needsPaste ? rowPaste(w, i) : cancel).left() - 5;
            text(QRect(14, top, end - 14, 18), job.names.isEmpty() ? tr("Transfer failed") : job.names.value(job.index), 12, "#e3ebf6", true);
            text(QRect(14, top + 17, end - 14, 16), status(job), 11, "#a6b3c6");
            if (needsPaste) action(rowPaste(w, i), tr("Paste here"));
            action(cancel, finished(job) ? tr("Dismiss") : tr("Cancel"));
        }
        if (m_Others.size() > otherRows()) {
            const int top = summaryHeight(m_Layout) + otherRows() * Row;
            action(QRect(14, top, w - 28, 23), tr("More tasks (%1-%2 of %3)").arg(m_First + 1).arg(m_First + otherRows()).arg(m_Others.size()));
        }
    }

    static constexpr int Margin = 12;
    static bool finished(const ClipboardUploadProgress& job) {
        using State = ClipboardUploadProgress::State;
        return job.state == State::Failed || job.state == State::ReadyLocally ||
               job.state == State::Saved || job.state == State::Cancelled;
    }
    static QRect toggleRect(int w) { return QRect(w - 40, 6, 28, 28); }
    static QRect cancelRect(int w) { return QRect(w - 132, 6, 86, 28); }
    Header headerAt(QPoint p, int w) const {
        if (!QRect(0, 0, w, 39).contains(p)) return Header::None;
        if (toggleRect(w).contains(p)) return Header::Toggle;
        if (!m_Collapsed && cancelRect(w).contains(p)) return Header::Cancel;
        if (!m_Collapsed && m_Upload.state == ClipboardUploadProgress::State::NeedsPaste && pasteRect(w).contains(p)) return Header::None;
        return Header::Drag;
    }
    SDL_Rect position(QSize size) const {
        auto rect = panelRect(m_Layout.width(), m_Layout.height(), size.width(), size.height());
        if (m_Position) {
            rect.x = qBound(0, m_Position->x(), qMax(0, m_Layout.width() - rect.w));
            rect.y = qBound(0, m_Position->y(), qMax(0, m_Layout.height() - rect.h));
        }
        return rect;
    }
    bool hasTask() const { return m_Upload.taskId != 0 || !m_Upload.names.isEmpty(); }
    static constexpr int Row = 36;
    static QRect pasteRect(int w) { return cancelRect(w).translated(-77, 0).adjusted(0, 0, -13, 0); }
    QRect rowCancel(int w, int i) const { return QRect(w - 72, summaryHeight(m_Layout) + i * Row + 3, 58, 27); }
    QRect rowPaste(int w, int i) const { return rowCancel(w, i).translated(-77, 0).adjusted(0, 0, 15, 0); }
    static QString status(const ClipboardUploadProgress& job) {
        using State = ClipboardUploadProgress::State;
        const auto tr = [](const char* s) { return QCoreApplication::translate("ClipboardTransfer", s); };
        switch (job.state) {
        case State::Queued: return tr("Queued");
        case State::NeedsPaste: return tr("Choose where to paste");
        case State::WaitingForPaste: return tr("Waiting for paste");
        case State::Saved: return tr("Files saved");
        case State::Cancelled: return tr("Transfer cancelled");
        case State::ReadyLocally: return tr("Ready to paste on this PC");
        case State::Failed: return job.error.isEmpty() ? tr("Transfer failed") : job.error;
        default: return tr("Waiting for host");
        }
    }
    int summaryHeight(QSize window) const {
        const bool sending = m_Upload.state == ClipboardUploadProgress::State::Uploading && !m_Upload.names.isEmpty();
        return (sending ? 80 : 44) + 21 * rows(window) + (sending && window.width() - Margin * 2 < 270 ? 17 : 0);
    }
    int otherRows() const {
        const int available = m_Layout.height() - 2 * Margin - summaryHeight(m_Layout);
        const int limit = qBound(1, available / Row, 4);
        if (m_Others.size() <= limit) return int(m_Others.size());
        return qBound(1, (available - 26) / Row, limit);
    }
    int rows(QSize window) const
    {
        return qMin(qBound(1, (window.height() - 95) / 21, 3), int(m_Upload.names.size()) - m_Upload.index);
    }
    QSize panelSize(QSize window) const
    {
        const int width = qBound(1, window.width() - Margin * 2, 332);
        if (m_Collapsed) return QSize(qMin(width, 240), 40);
        const int extra = otherRows() * Row + (otherRows() < m_Others.size() ? 26 : 0);
        return QSize(width, summaryHeight(window) + extra);
    }

    mutable QReadWriteLock m_Lock;
    ClipboardUploadProgress m_Upload;
    QList<ClipboardUploadProgress> m_Others;
    int m_First = 0;
    TransferRateSampler m_Rate;
    qint64 m_BytesPerSecond = -1;
    QSize m_Window;
    QSize m_Layout;
    qreal m_Scale = 1;
    Header m_Hover = Header::None;
    bool m_Collapsed = false;
    std::optional<QPoint> m_Position;
    QPoint m_Grab;
    bool m_MouseDrag = false;
    std::optional<QPair<SDL_TouchID, SDL_FingerID>> m_DragFinger;
    bool m_Escape = false;
    Uint32 m_Buttons = 0;
    QSet<QPair<SDL_TouchID, SDL_FingerID>> m_Fingers;
};
