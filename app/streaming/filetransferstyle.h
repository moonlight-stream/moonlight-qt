#pragma once

#include <QEvent>
#include <QFont>
#include <QGuiApplication>
#include <QHeaderView>
#include <QIcon>
#include <QPainter>
#include <QPalette>
#include <QStyle>
#include <QTreeWidget>
#include <qt_windows.h>
#include "windowcaption.h"

namespace FileTransferStyle {

inline QFont headingFont(QFont font)
{
    if (font.pointSizeF() > 0) font.setPointSizeF(font.pointSizeF() + 2);
    else font.setPixelSize(font.pixelSize() + 3);
    font.setWeight(QFont::DemiBold);
    return font;
}

inline void configureList(QTreeWidget* tree, const QStringList& labels)
{
    tree->setHeaderLabels(labels);
    tree->setRootIsDecorated(false);
    tree->setUniformRowHeights(true);
    tree->setAlternatingRowColors(true);
    tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tree->header()->setStretchLastSection(false);
    tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
}

inline bool highContrastEnabled()
{
    HIGHCONTRASTW contrast{sizeof(HIGHCONTRASTW)};
    return SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast), &contrast, 0) &&
           (contrast.dwFlags & HCF_HIGHCONTRASTON);
}

// Qt styles the client area. Windows gives each top-level window, including owned dialogs,
// its own native caption when it creates the HWND.
class CaptionFilter final : public QObject
{
protected:
    bool eventFilter(QObject* object, QEvent* event) override
    {
        if (event->type() != QEvent::Show && event->type() != QEvent::WinIdChange) return false;
        auto* window = qobject_cast<QWidget*>(object);
        if (!window || (window->windowType() != Qt::Window && window->windowType() != Qt::Dialog)) return false;
        const auto hwnd = reinterpret_cast<HWND>(window->internalWinId());
        if (!hwnd) return false;
        for (auto* owner = window; owner; owner = owner->parentWidget()) {
            if (owner->objectName() == QLatin1String("transferWindow")) {
                if (!highContrastEnabled()) applyLightWindowCaption(hwnd);
                break;
            }
        }
        return false;
    }
};

inline QIcon icon(const QWidget* widget, const QString& name, QStyle::StandardPixmap fallback)
{
    const auto nativeIcon = QIcon::fromTheme(name);
    if (nativeIcon.isNull()) return widget->style()->standardIcon(fallback, nullptr, widget);
    if (highContrastEnabled()) return nativeIcon;
    // The Windows icon engine picks its foreground from the application-wide system theme.
    // Tint only the toolbar glyphs; file and folder icons keep their shell colors.
    auto glyph = nativeIcon.pixmap(64, 64);
    if (glyph.isNull()) return nativeIcon;
    QPainter painter(&glyph);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(glyph.rect(), QColor("#385e8a"));
    painter.end();
    return QIcon(glyph);
}

inline void apply(QWidget* window)
{
    if (highContrastEnabled()) return;

    // Match the light device and settings pages without changing the streaming window's palette.
    QPalette palette(QColor("#eff4f7"));
    palette.setColor(QPalette::Window, QColor("#f7f9fc"));
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::AlternateBase, QColor("#f6f8fb"));
    palette.setColor(QPalette::Midlight, QColor("#dce3ea"));
    palette.setColor(QPalette::Mid, QColor("#c5d4e6"));
    palette.setColor(QPalette::Highlight, QColor("#2468d5"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    for (const auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText}) {
        palette.setColor(role, QColor("#233449"));
        palette.setColor(QPalette::Disabled, role, QColor("#66778b"));
    }
    window->setPalette(palette);
    window->setObjectName("transferWindow");
    if (QGuiApplication::platformName() == QLatin1String("windows")) {
        static CaptionFilter captions;
        qGuiApp->installEventFilter(&captions);
    }
    auto font = window->font();
    font.setFamilies({QStringLiteral("Segoe UI"), QStringLiteral("Microsoft YaHei UI")});
    window->setFont(font);

    window->setStyleSheet(QStringLiteral(R"(
        QWidget#transferWindow, QDialog { background: #f7f9fc; color: #233449; }
        QLabel, QLineEdit, QMenu, QComboBox, QAbstractItemView, QHeaderView, QPushButton, QToolButton {
            color: #233449; font-family: "Segoe UI", "Microsoft YaHei UI";
        }
        QFrame#transferPanel, QFrame#transferAddress, QTreeWidget {
            background: white; border: 1px solid #dce3ea; border-radius: 5px;
        }
        QLineEdit, QAbstractItemView, QMenu, QComboBox {
            background: white; selection-background-color: #2468d5; selection-color: white;
        }
        QTreeWidget { alternate-background-color: #f6f8fb; }
        QSplitter::handle { background: #f7f9fc; }
        QSplitter::handle:hover { background: #c5d4e6; }
        QTreeView::item { min-height: 28px; border: none; }
        QTreeView::item:selected, QMenu::item:selected { background: #dce8fa; color: #182b46; }
        QTreeWidget::indicator:unchecked, QCheckBox::indicator:unchecked {
            background: white; border: 1px solid #748397; border-radius: 3px;
        }
        QCheckBox::indicator:unchecked:disabled { background: #f6f8fb; border-color: #c5d4e6; }
        QLabel#transferContext { color: #52647a; }
        QHeaderView::section {
            background: #f3f6fa; color: #415168;
            border: none; border-bottom: 1px solid #dce3ea; padding: 4px 6px;
        }
        QPushButton, QToolButton#transferMenu {
            background: white; border: 1px solid #c5d4e6; border-radius: 5px; padding: 5px 10px;
        }
        QToolButton#transferMenu { padding: 4px 17px 4px 9px; color: #415168; border-color: #dce3ea; }
        QToolButton#transferMenu::menu-indicator {
            subcontrol-origin: padding; subcontrol-position: center right;
            right: 5px; width: 11px; height: 8px;
        }
        QPushButton#transferCommand { text-align: left; padding: 8px 12px; }
        QToolButton#transferTool {
            background: transparent; border: 1px solid transparent; border-radius: 5px; padding: 3px;
        }
        QToolButton#transferTool:checked { color: #2468d5; }
        QPushButton:enabled:hover, QToolButton#transferMenu:enabled:hover, QToolButton#transferTool:enabled:hover,
        QPushButton:enabled:focus, QToolButton#transferMenu:focus, QToolButton#transferTool:focus,
        QToolButton#transferTool:checked { background: #e6effb; border-color: #2468d5; }
        QTreeWidget:focus { border-color: #2468d5; }
        QPushButton:enabled:pressed, QToolButton#transferMenu:pressed, QToolButton#transferTool:pressed { background: #dce8fa; }
        QPushButton#transferPrimary:enabled { background: #2468d5; color: white; }
        QPushButton#transferPrimary:enabled:hover, QPushButton#transferPrimary:enabled:pressed { background: #1e59b8; }
        QPushButton#transferPrimary:focus { border-color: #182b46; }
        QPushButton:disabled, QToolButton#transferMenu:disabled {
            background: #f6f8fb; color: #66778b; border-color: #dce3ea;
        }
        QMessageBoxDetailsText > QTextEdit {
            background: white; color: #233449; border: 1px solid #dce3ea; border-radius: 5px;
            selection-background-color: #2468d5; selection-color: white;
        }
        QMessageBoxDetailsText > QFrame[frameShape="HLine"] { border: none; border-top: 1px solid #dce3ea; }
        QFrame#transferDivider { background: #dce3ea; border: none; }
        QScrollBar { background: transparent; border: none; }
        QScrollBar:vertical { width: 16px; }
        QScrollBar:horizontal { height: 16px; }
        QScrollBar::handle { background: #8093aa; border-radius: 4px; }
        QScrollBar::handle:vertical { min-height: 24px; margin: 0 4px; }
        QScrollBar::handle:horizontal { min-width: 24px; margin: 4px 0; }
        QScrollBar::handle:hover { background: #597ba2; }
        QScrollBar::handle:pressed { background: #2468d5; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
        QScrollBar::add-line, QScrollBar::sub-line { width: 0; height: 0; }
    )"));
}

}
