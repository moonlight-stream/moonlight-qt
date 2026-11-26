#pragma once

#include <QPainter>
#include <QPainterPath>
#include <QVector>

// The two panels share one non-activating HWND. Row coordinates are in DIPs,
// and frame is in native pixels relative to that window.
namespace CaptionMenu {
enum class Page { Main, Display, Quality, Sound, Security, MouseMode, SystemKeys, Fields, Corner, Opacity, PrivacyResolution };
enum class Action { Submenu, Back, Info, Divider, Hud, Field, Corner, Opacity, MouseMode, SystemKeys,
                    SetQuality, BitrateSlider, SelectDisplay, ResizePrivacy, HostMute, DisconnectLock, Privacy, LockNow, Fullscreen, Files, Disconnect };
enum class Icon { None, Display, Quality, Sound, Security, Mouse, Keyboard, Files, Fullscreen, Disconnect };

inline QFont font(int pixels)
{
    QFont font(QStringLiteral("Microsoft YaHei UI"));
    font.setPixelSize(pixels);
    return font;
}

struct Row {
    Action action;
    QString label;
    QString detail;
    Page target = Page::Main;
    int value = 0;
    Icon icon = Icon::None;
    bool checked = false;
    bool enabled = true;
    int top = 0;

    bool selectable() const { return enabled && action != Action::Info && action != Action::Divider; }
    int sliderMaximum = 0;
    static constexpr int SliderMin = 500;
    static constexpr int SliderInset = 18;
    int height() const { return action == Action::Divider ? 5 : action == Action::BitrateSlider ? 56 : 34; }
    int sliderValue(qreal x, int width) const
    {
        const qreal fraction = qBound(0.0, (x - SliderInset) / (width - 2 * SliderInset), 1.0);
        return SliderMin + 500 * qRound(fraction * (sliderMaximum - SliderMin) / 500);
    }
};

struct Panel {
    static constexpr int Padding = 4;
    QVector<Row> rows;
    QString footer;
    QRect frame;
    QSize size;
    int hover = -1;
    int scroll = 0;
    int footerTop = 0;
    int contentHeight = 0;

    Row& add(Action action, const QString& label = {}, Icon icon = Icon::None)
    {
        rows.append(Row{action, label});
        rows.last().icon = icon;
        return rows.last();
    }
    Row& submenu(Page page, const QString& label, Icon icon = Icon::None)
    {
        auto& row = add(Action::Submenu, label, icon);
        row.target = page;
        return row;
    }
    void choice(Action action, const QString& label, int value, bool checked)
    {
        auto& row = add(action, label);
        row.value = value;
        row.checked = checked;
    }
    void measure(int width, int maxHeight)
    {
        int y = Padding;
        for (auto& row : rows) {
            row.top = y;
            y += row.height();
        }
        footerTop = y;
        if (!footer.isEmpty()) {
            const QFontMetrics metrics(font(12));
            y += 16 + metrics.boundingRect(QRect(0, 0, qMax(1, width - 32), 0),
                                           Qt::TextWordWrap, footer).height();
        }
        contentHeight = y + Padding;
        size = QSize(width, qMin(contentHeight, maxHeight));
        scroll = qBound(0, scroll, contentHeight - size.height());
    }
    int rowAt(QPoint point) const
    {
        if (!QRect(QPoint(), size).adjusted(Padding, Padding, -Padding, -Padding).contains(point))
            return -1;
        for (int i = 0; i < rows.size(); ++i) {
            const auto& row = rows[i];
            const int y = point.y() + scroll - row.top;
            if (row.selectable() && y >= 0 && y < row.height())
                return i;
        }
        return -1;
    }
    void moveSelection(int direction)
    {
        int i = hover < 0 && direction < 0 ? 0 : hover;
        for (int n = 0; n < rows.size(); ++n) {
            i = (i + direction + rows.size()) % rows.size();
            const auto& row = rows[i];
            if (row.selectable()) {
                hover = i;
                if (row.top < scroll + Padding)
                    scroll = row.top - Padding;
                else if (row.top + row.height() > scroll + size.height() - Padding)
                    scroll = row.top + row.height() + Padding - size.height();
                return;
            }
        }
    }

    static void paintIcon(QPainter& p, Icon icon, int cy, QColor color)
    {
        p.save();
        p.translate(16, cy - 8);
        p.setPen(QPen(color, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        switch (icon) {
        case Icon::Display:
            p.drawRoundedRect(QRectF(1, 2, 14, 12), 1.8, 1.8);
            p.drawLine(QPointF(2, 12.5), QPointF(14, 12.5));
            break;
        case Icon::Quality:
            p.drawRoundedRect(QRectF(1, 2, 14, 12), 2, 2);
            p.drawEllipse(QPointF(5, 6), 1, 1);
            p.drawPolyline(QPolygonF{{2, 12}, {6, 9}, {8, 10.5}, {11, 7}, {15, 11}});
            break;
        case Icon::Sound:
            p.drawPolygon(QPolygonF{{1, 6}, {4, 6}, {8, 2}, {8, 14}, {4, 10}, {1, 10}});
            p.drawArc(QRectF(6, 4, 6, 8), -48 * 16, 96 * 16);
            p.drawArc(QRectF(4, 0, 11, 16), -42 * 16, 84 * 16);
            break;
        case Icon::Security: {
            QPainterPath shield;
            shield.moveTo(8, 1); shield.lineTo(14.5, 3.5); shield.lineTo(14, 8.5);
            shield.quadTo(13.5, 12, 8, 15); shield.quadTo(2.5, 12, 2, 8.5);
            shield.lineTo(1.5, 3.5); shield.closeSubpath();
            p.drawPath(shield);
            p.drawPolyline(QPolygonF{{9, 5}, {6.5, 8}, {9.5, 8}, {7, 11}});
            break;
        }
        case Icon::Mouse:
            p.drawRoundedRect(QRectF(3, 1, 10, 14), 5, 5);
            p.drawLine(QPointF(8, 4), QPointF(8, 7));
            break;
        case Icon::Keyboard:
            p.drawRoundedRect(QRectF(0.8, 3, 14.4, 10), 1.6, 1.6);
            for (int i = 0; i < 4; ++i) p.drawPoint(QPointF(3.5 + 3 * i, 6.5));
            p.drawLine(QPointF(5, 10), QPointF(11, 10));
            break;
        case Icon::Files:
            p.drawPolyline(QPolygonF{{6, 13}, {1, 13}, {1, 3}, {6, 3}, {8, 5}, {15, 5}, {15, 7}});
            p.drawLine(QPointF(7, 11), QPointF(15, 11));
            p.drawPolyline(QPolygonF{{12, 8}, {15, 11}, {12, 14}});
            break;
        case Icon::Fullscreen:
            p.translate(8, 8);
            for (int i = 0; i < 4; ++i) {
                p.drawPolyline(QPolygonF{{-6, -2.5}, {-6, -6}, {-2.5, -6}});
                p.drawLine(QPointF(-6, -6), QPointF(-2, -2));
                p.rotate(90);
            }
            break;
        case Icon::Disconnect:
            p.drawPolyline(QPolygonF{{8, 2}, {1.5, 2}, {1.5, 14}, {8, 14}});
            p.drawLine(QPointF(6, 8), QPointF(15, 8));
            p.drawPolyline(QPolygonF{{12, 5}, {15, 8}, {12, 11}});
            break;
        case Icon::None: break;
        }
        p.restore();
    }

    void paint(QPainter& p, qreal scale, Page active) const
    {
        if (frame.isEmpty()) return;
        p.save();
        p.translate(frame.topLeft());
        p.scale(scale, scale);
        const QRectF surface(0, 0, frame.width() / scale, frame.height() / scale);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(QColor("#c7c9cc"), 1 / scale));
        p.setBrush(QColor("#f8f9fa"));
        p.drawRoundedRect(surface.adjusted(0.5 / scale, 0.5 / scale, -0.5 / scale, -0.5 / scale), 7, 7);
        p.setClipRect(surface.adjusted(1, Padding, -1, -Padding), Qt::IntersectClip);
        p.translate(0, -scroll);
        const auto normal = font(14);
        const auto detailFont = font(12);
        const QColor ink("#202124"), muted("#606469"), rule("#dcdedf"), danger("#c43829");
        const QRectF dirty = p.clipBoundingRect();
        for (int i = 0; i < rows.size(); ++i) {
            const auto& row = rows[i];
            QRect line(Padding, row.top, size.width() - 2 * Padding, row.height());
            if (!dirty.intersects(line)) continue;
            const int cy = line.y() + line.height() / 2;
            if (row.action == Action::Divider) {
                p.fillRect(QRectF(1, cy, size.width() - 2, 1 / scale), rule);
                continue;
            }
            if ((i == hover && row.selectable()) || (row.action == Action::Submenu && row.target == active)) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor("#eaecee"));
                p.drawRoundedRect(line.adjusted(1, 2, -1, -2), 4, 4);
            }
            const QColor color = !row.enabled ? QColor("#85898d") : (row.action == Action::Disconnect ? danger : ink);
            p.setPen(color);
            if (row.action == Action::BitrateSlider) {
                const QRect title = line.adjusted(Row::SliderInset - Padding, 0, Padding - Row::SliderInset, -28);
                p.setFont(normal);
                p.drawText(title, Qt::AlignVCenter | Qt::AlignLeft, row.label);
                p.drawText(title, Qt::AlignVCenter | Qt::AlignRight, row.detail);
                const int y = line.top() + 38;
                const int left = Row::SliderInset, right = size.width() - Row::SliderInset;
                const qreal ratio = qBound(0.0, qreal(row.value - Row::SliderMin) / (row.sliderMaximum - Row::SliderMin), 1.0);
                const qreal x = left + (right - left) * ratio;
                const QColor accent = row.enabled ? QColor("#2468d5") : muted;
                p.setPen(QPen(rule, 3, Qt::SolidLine, Qt::RoundCap));
                p.drawLine(left, y, right, y);
                p.setPen(QPen(accent, 3, Qt::SolidLine, Qt::RoundCap));
                p.drawLine(QPointF(left, y), QPointF(x, y));
                p.setPen(Qt::NoPen);
                p.setBrush(accent);
                p.drawEllipse(QPointF(x, y), 6, 6);
                continue;
            }
            if (row.icon != Icon::None) {
                paintIcon(p, row.icon, cy, color);
            } else if (row.checked) {
                p.setPen(Qt::NoPen);
                p.setBrush(color);
                p.drawEllipse(QPointF(24, cy), 2.3, 2.3);
            }
            if (row.action == Action::Submenu || row.action == Action::Back) {
                const bool back = row.action == Action::Back;
                const qreal x = back ? 25 : size.width() - 19;
                const qreal dx = back ? -3.5 : 3.5;
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(muted, 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                p.drawPolyline(QPolygonF{{x - dx / 2, cy - 4.0}, {x + dx / 2, qreal(cy)}, {x - dx / 2, cy + 4.0}});
            }
            QRect label = line.adjusted(40, 0, row.action == Action::Submenu ? -29 : -11, 0);
            if (!row.detail.isEmpty()) {
                p.setFont(detailFont);
                p.setPen(muted);
                const int width = qBound(0, p.fontMetrics().horizontalAdvance(row.detail) + 3,
                                         qMax(0, label.width() - 55));
                QRect detail = label;
                detail.setLeft(detail.right() - width + 1);
                p.drawText(detail, Qt::AlignVCenter | Qt::AlignRight,
                           p.fontMetrics().elidedText(row.detail, Qt::ElideRight, width));
                label.setRight(detail.left() - 8);
            }
            p.setFont(normal);
            p.setPen(color);
            p.drawText(label, Qt::AlignVCenter | Qt::AlignLeft,
                       p.fontMetrics().elidedText(row.label, Qt::ElideRight, label.width()));
        }
        if (!footer.isEmpty()) {
            p.fillRect(QRectF(1, footerTop, size.width() - 2, 1 / scale), rule);
            p.setFont(detailFont);
            p.setPen(muted);
            p.drawText(QRect(16, footerTop + 8, size.width() - 32, contentHeight - footerTop - 8),
                       Qt::TextWordWrap, footer);
        }
        if (size.height() < contentHeight) {
            const int track = size.height() - 2 * Padding;
            const int thumb = qMax(8, track * size.height() / contentHeight);
            const int y = Padding + (track - thumb) * scroll / (contentHeight - size.height());
            p.fillRect(QRect(size.width() - 4, scroll + y, 2, thumb), QColor("#a6a9ac"));
        }
        p.restore();
    }
};
}
