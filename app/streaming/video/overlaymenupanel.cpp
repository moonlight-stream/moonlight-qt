#include "overlaymenupanel.h"
#include "uifont.h"

#include <QScreen>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QPainterPath>
#include <QCursor>
#include <QFontDatabase>
#include <QFontMetrics>
#include <memory>

namespace {
constexpr qint64 PointerGracePeriodMs = 300;
constexpr int PointerCheckIntervalMs = 150;
}

OverlayMenuPanel::OverlayMenuPanel(QWindow* parent)
    : QRasterWindow(parent),
      m_CurrentLevel(0),
      m_HoveredIndex(-1),
      m_Visible(false),
      m_HasGamepads(false),
      m_FileMappingState(FileMappingState::Unknown),
      m_FileMappingDetail(tr("Checking")),
      m_ParentX(0), m_ParentY(0), m_ParentW(0), m_ParentH(0),
      m_CloseWhenPointerOutside(false),
      m_ContentOffset(0),
      m_Closing(false),
      m_TargetPosition(),
      m_AnchorMode(AnchorMode::RightEdge),
      m_TriggerPosition(std::nullopt)
{
    setFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
             | Qt::WindowDoesNotAcceptFocus);

    QSurfaceFormat fmt;
    fmt.setAlphaBufferSize(8);
    setFormat(fmt);

    // Logical (unscaled) values — Qt 6 handles DPI automatically
    // Win11 dark context menu style
    m_ItemHeight   = 38;
    m_Padding      = 4;
    m_MenuWidth    = 280;
    m_BorderRadius = 8;
    m_ShadowMargin = 8;
    m_TitleHeight  = 32;
    m_IconAreaWidth = 24;

    // Load ModeSeven.ttf (same font as performance stats overlay)
    int fontId = QFontDatabase::addApplicationFont(QStringLiteral(":/data/ModeSeven.ttf"));
    QString modeSeven;
    if (fontId >= 0) {
        QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty())
            modeSeven = families.first();
    }

    m_LabelFont.setFamilies(UiFont::familyChain(modeSeven));
    m_LabelFont.setPointSize(9);
    m_LabelFont.setWeight(QFont::Normal);

    m_DetailFont = QFont(m_LabelFont);
    m_DetailFont.setPointSize(8);
    m_DetailFont.setWeight(QFont::Normal);

    m_TitleFont = QFont(m_LabelFont);
    m_TitleFont.setPointSize(8);
    m_TitleFont.setWeight(QFont::DemiBold);

    // Icon font: platform-specific
#ifdef Q_OS_WIN
    // Segoe MDL2 Assets — available on Windows 10/11
    m_IconFont = QFont(QStringLiteral("Segoe MDL2 Assets"), 10);
#else
    // Material Icons (bundled, Apache 2.0) — cross-platform fallback
    {
        int iconFontId = QFontDatabase::addApplicationFont(QStringLiteral(":/data/MaterialIcons-Regular.ttf"));
        QString materialFamily;
        if (iconFontId >= 0) {
            QStringList families = QFontDatabase::applicationFontFamilies(iconFontId);
            if (!families.isEmpty())
                materialFamily = families.first();
        }
        if (!materialFamily.isEmpty())
            m_IconFont = QFont(materialFamily, 12);
        else
            m_IconFont = QFont(QStringLiteral("Material Icons"), 12);
    }
#endif
    m_IconFont.setWeight(QFont::Normal);

    // --- Animations ---
    m_OpacityAnim = new QPropertyAnimation(this, "opacity", this);
    m_SlideAnim   = new QPropertyAnimation(this, "x", this);

    m_ContentSlideAnim = new QVariantAnimation(this);
    m_ContentSlideAnim->setDuration(150);
    m_ContentSlideAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_ContentSlideAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& val) {
        m_ContentOffset = val.toReal();
        forceRepaint();
    });
    connect(m_ContentSlideAnim, &QVariantAnimation::finished, this, [this]() {
        m_ContentOffset = 0;
        forceRepaint();
    });

    m_LeaveTimer.setSingleShot(true);
    connect(&m_LeaveTimer, &QTimer::timeout, this, [this]() {
        if (!m_Visible || !m_CloseWhenPointerOutside) {
            return;
        }

        const QRect contentGeometry = geometry().adjusted(
                m_ShadowMargin, m_ShadowMargin,
                -m_ShadowMargin, -m_ShadowMargin);
        if (!contentGeometry.contains(QCursor::pos())) {
            closeMenu();
        }
        else {
            schedulePointerOutsideCheck();
        }
    });

    buildMenuLevels();
}

OverlayMenuPanel::~OverlayMenuPanel()
{
}

// ---------------------------------------------------------------------------
// Synchronous repaint — requestUpdate() is async on Windows and may delay
// the visual update by up to 1 second (until the next SDL event arrives).
// This method directly delivers an UpdateRequest event so the paintEvent()
// runs immediately within the current call frame.
// ---------------------------------------------------------------------------

void OverlayMenuPanel::forceRepaint()
{
    if (isExposed()) {
        // Mark entire window as dirty (sets the dirty region for QPaintDeviceWindow)
        update(QRect(0, 0, width(), height()));
        // Synchronously deliver UpdateRequest to trigger paintEvent + backing store flush
        QEvent ev(QEvent::UpdateRequest);
        QCoreApplication::sendEvent(this, &ev);
    }
}

// ---------------------------------------------------------------------------
// Menu structure
// ---------------------------------------------------------------------------

void OverlayMenuPanel::buildMenuLevels()
{
    m_MenuLevels.clear();

    // === Level 0: Top-level categories ===
    MenuLevel top;
    top.title = tr("Overlay Menu");
    top.items.push_back({tr("Quick Actions"), QString(),  MenuItemType::SubMenu,
                         MenuAction::MenuActionMax, 1, true, false, false});
    top.items.push_back({tr("Menu Position"), QString(), MenuItemType::SubMenu,
                         MenuAction::MenuActionMax, 3, true, false, false});
    top.items.push_back({tr("Bitrate"),       QString(),  MenuItemType::SubMenu,
                         MenuAction::MenuActionMax, 2, true, false, false});
    top.items.push_back({tr("Host Files"),    m_FileMappingDetail, MenuItemType::Action,
                         MenuAction::ShowHostFiles, 0, true,
                         m_FileMappingState == FileMappingState::Available ||
                         m_FileMappingState == FileMappingState::Open, true});   // separator
    top.items.push_back({tr("Toggle Fullscreen"), QString(), MenuItemType::Action,
                         MenuAction::ToggleFullScreen, 0, true, false, false});
    top.items.push_back({tr("Microphone"),    QString(),  MenuItemType::Toggle,
                         MenuAction::ToggleMicrophone, 0, true, false, !m_HasGamepads}); // separator if no gamepad item follows
    // Only show Gamepad Mouse toggle when a gamepad is actually connected
    if (m_HasGamepads) {
        top.items.push_back({tr("Gamepad Mouse"), QString(),  MenuItemType::Toggle,
                             MenuAction::ToggleGamepadMouse, 0, true, false, true}); // separator
    }
    top.items.push_back({tr("Disconnect"),    QString(),  MenuItemType::Action,
                         MenuAction::Quit, 0, true, false, false});
    m_MenuLevels.push_back(top);

    // === Level 1: Quick Actions (keyboard shortcuts) ===
    MenuLevel shortcuts;
    shortcuts.title = tr("Quick Actions");
    shortcuts.items.push_back({tr("Quit Moonlight"),      "Ctrl+Alt+Shift+E", MenuItemType::Action,
                               MenuAction::QuitAndExit,           0, true, false, true});
    shortcuts.items.push_back({tr("Performance Stats"),   "Ctrl+Alt+Shift+S", MenuItemType::Action,
                               MenuAction::ToggleStatsOverlay,    0, true, false, true});
    shortcuts.items.push_back({tr("Mouse Mode"),          "Ctrl+Alt+Shift+M", MenuItemType::Action,
                               MenuAction::ToggleMouseMode,       0, true, false, false});
    shortcuts.items.push_back({tr("Show/Hide Cursor"),    "Ctrl+Alt+Shift+C", MenuItemType::Action,
                               MenuAction::ToggleCursorHide,      0, true, false, false});
    shortcuts.items.push_back({tr("Minimize"),            "Ctrl+Alt+Shift+D", MenuItemType::Action,
                               MenuAction::ToggleMinimize,        0, true, false, true});
    shortcuts.items.push_back({tr("Ungrab Mouse"),        "Ctrl+Alt+Shift+Z", MenuItemType::Action,
                               MenuAction::UngrabInput,           0, true, false, false});
    shortcuts.items.push_back({tr("Paste Clipboard"),     "Ctrl+Alt+Shift+V", MenuItemType::Action,
                               MenuAction::PasteText,             0, true, false, false});
    shortcuts.items.push_back({tr("Pointer Region Lock"), "Ctrl+Alt+Shift+L", MenuItemType::Action,
                               MenuAction::TogglePointerRegionLock, 0, true, false, false});
    m_MenuLevels.push_back(shortcuts);

    // === Level 2: Bitrate presets ===
    MenuLevel bitrate;
    bitrate.title = tr("Bitrate");
    bitrate.items.push_back({tr("1 Mbps"),    QString(), MenuItemType::Action,
                             MenuAction::SetBitrate1000,   0, true, false, false});
    bitrate.items.push_back({tr("2 Mbps"),    QString(), MenuItemType::Action,
                             MenuAction::SetBitrate2000,   0, true, false, false});
    bitrate.items.push_back({tr("5 Mbps"),    QString(), MenuItemType::Action,
                             MenuAction::SetBitrate5000,   0, true, false, false});
    bitrate.items.push_back({tr("10 Mbps"),   QString(), MenuItemType::Action,
                             MenuAction::SetBitrate10000,  0, true, false, false});
    bitrate.items.push_back({tr("20 Mbps"),   QString(), MenuItemType::Action,
                             MenuAction::SetBitrate20000,  0, true, false, false});
    bitrate.items.push_back({tr("30 Mbps"),   QString(), MenuItemType::Action,
                             MenuAction::SetBitrate30000,  0, true, false, false});
    bitrate.items.push_back({tr("50 Mbps"),   QString(), MenuItemType::Action,
                             MenuAction::SetBitrate50000,  0, true, false, false});
    bitrate.items.push_back({tr("100 Mbps"),  QString(), MenuItemType::Action,
                             MenuAction::SetBitrate100000, 0, true, false, false});
    m_MenuLevels.push_back(bitrate);

    // === Level 3: Overlay menu placement ===
    MenuLevel placement;
    placement.title = tr("Menu Position");
    placement.items.push_back({tr("Top edge"), QString(), MenuItemType::Action,
                               MenuAction::SetMenuPlacementTop, 0, true, false, false});
    placement.items.push_back({tr("Right edge"), QString(), MenuItemType::Action,
                               MenuAction::SetMenuPlacementRight, 0, true, false, false});
    placement.items.push_back({tr("Left edge"), QString(), MenuItemType::Action,
                               MenuAction::SetMenuPlacementLeft, 0, true, false, false});
    placement.items.push_back({tr("Floating button"), QString(), MenuItemType::Action,
                               MenuAction::SetMenuPlacementButton, 0, true, false, false});
    placement.items.push_back({tr("Disabled"), QString(), MenuItemType::Action,
                               MenuAction::SetMenuPlacementDisabled, 0, true, false, false});
    m_MenuLevels.push_back(placement);
}

// ---------------------------------------------------------------------------
// Dynamic state updates
// ---------------------------------------------------------------------------

void OverlayMenuPanel::updateMicrophoneState(bool enabled)
{
    if (m_MenuLevels.empty()) return;
    for (auto& item : m_MenuLevels[0].items) {
        if (item.action == MenuAction::ToggleMicrophone) {
            item.toggleState = enabled;
            forceRepaint();
            break;
        }
    }
}

void OverlayMenuPanel::updateGamepadMouseState(bool enabled)
{
    if (m_MenuLevels.empty()) return;
    for (auto& item : m_MenuLevels[0].items) {
        if (item.action == MenuAction::ToggleGamepadMouse) {
            item.toggleState = enabled;
            forceRepaint();
            break;
        }
    }
}

void OverlayMenuPanel::updateBitrateState(int bitrateKbps)
{
    if (m_MenuLevels.empty()) return;

    // Show current bitrate as detail text on the Bitrate category (level 0)
    for (auto& item : m_MenuLevels[0].items) {
        if (item.type == MenuItemType::SubMenu && item.targetLevel == 2) {
            if (bitrateKbps >= 1000) {
                item.detail = QString("%1 Mbps").arg(bitrateKbps / 1000);
            } else {
                item.detail = QString("%1 kbps").arg(bitrateKbps);
            }
            break;
        }
    }

    // Mark the active bitrate preset in level 2
    if ((int)m_MenuLevels.size() > 2) {
        auto actionToKbps = [](MenuAction a) -> int {
            switch (a) {
            case MenuAction::SetBitrate1000:   return 1000;
            case MenuAction::SetBitrate2000:   return 2000;
            case MenuAction::SetBitrate5000:   return 5000;
            case MenuAction::SetBitrate10000:  return 10000;
            case MenuAction::SetBitrate20000:  return 20000;
            case MenuAction::SetBitrate30000:  return 30000;
            case MenuAction::SetBitrate50000:  return 50000;
            case MenuAction::SetBitrate100000: return 100000;
            default: return -1;
            }
        };
        for (auto& item : m_MenuLevels[2].items) {
            if (item.type == MenuItemType::Action) {
                int kbps = actionToKbps(item.action);
                item.detail = (kbps == bitrateKbps) ? QString::fromUtf8("\342\234\223") : QString();
            }
        }
    }
}

void OverlayMenuPanel::updateMenuPositionState(MenuAction activePlacementAction)
{
    if (m_MenuLevels.size() <= 3) {
        return;
    }

    QString activeLabel;
    for (auto& item : m_MenuLevels[3].items) {
        const bool active = item.action == activePlacementAction;
        item.detail = active ? QString::fromUtf8("\342\234\223") : QString();
        if (active) {
            activeLabel = item.label;
        }
    }

    for (auto& item : m_MenuLevels[0].items) {
        if (item.type == MenuItemType::SubMenu && item.targetLevel == 3) {
            item.detail = activeLabel;
            break;
        }
    }
    forceRepaint();
}

void OverlayMenuPanel::updateFileMappingState(FileMappingState state, const QString& detail)
{
    m_FileMappingState = state;
    m_FileMappingDetail = detail;

    if (m_MenuLevels.empty()) return;
    for (auto& item : m_MenuLevels[0].items) {
        if (item.action == MenuAction::ShowHostFiles) {
            item.detail = detail;
            item.toggleState = state == FileMappingState::Available ||
                               state == FileMappingState::Open;
            forceRepaint();
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Show / hide / navigate
// ---------------------------------------------------------------------------

void OverlayMenuPanel::showAtRightEdge(int parentX, int parentY, int parentW, int parentH,
                                       std::optional<QPoint> pointerGlobalPosition,
                                       bool closeWhenPointerOutside)
{
    m_AnchorMode = AnchorMode::RightEdge;
    m_ParentX = parentX;
    m_ParentY = parentY;
    m_ParentW = parentW;
    m_ParentH = parentH;
    m_TriggerPosition = pointerGlobalPosition;
    m_CloseWhenPointerOutside = closeWhenPointerOutside && pointerGlobalPosition.has_value();
    showInternal();
}

void OverlayMenuPanel::showAtLeftEdge(int parentX, int parentY, int parentW, int parentH,
                                      std::optional<QPoint> pointerGlobalPosition,
                                      bool closeWhenPointerOutside)
{
    m_AnchorMode = AnchorMode::LeftEdge;
    m_ParentX = parentX;
    m_ParentY = parentY;
    m_ParentW = parentW;
    m_ParentH = parentH;
    m_TriggerPosition = pointerGlobalPosition;
    m_CloseWhenPointerOutside = closeWhenPointerOutside && pointerGlobalPosition.has_value();
    showInternal();
}

void OverlayMenuPanel::showAtTopEdge(int parentX, int parentY, int parentW, int parentH,
                                     std::optional<QPoint> pointerGlobalPosition,
                                     bool closeWhenPointerOutside)
{
    m_AnchorMode = AnchorMode::TopEdge;
    m_ParentX = parentX;
    m_ParentY = parentY;
    m_ParentW = parentW;
    m_ParentH = parentH;
    m_TriggerPosition = pointerGlobalPosition;
    m_CloseWhenPointerOutside = closeWhenPointerOutside && pointerGlobalPosition.has_value();
    showInternal();
}

void OverlayMenuPanel::showAtCursor(int parentX, int parentY, int parentW, int parentH,
                                    const QPoint& cursorPosition, bool pointerTriggered)
{
    m_AnchorMode = AnchorMode::AtCursor;
    m_ParentX = parentX;
    m_ParentY = parentY;
    m_ParentW = parentW;
    m_ParentH = parentH;
    m_TriggerPosition = cursorPosition;
    m_CloseWhenPointerOutside = pointerTriggered;
    showInternal();
}

void OverlayMenuPanel::showInternal()
{
    m_LeaveTimer.stop();
    m_CurrentLevel = 0;
    m_HoveredIndex = -1;
    m_ContentOffset = 0;

    // If closing animation is in progress, cancel it
    if (m_Closing) {
        m_OpacityAnim->stop();
        m_SlideAnim->stop();
        m_Closing = false;
    }

    m_Visible = true;
    m_ShowTimer.start();

    // Calculate target geometry
    repositionWindow();
    m_TargetPosition = position();

    // Slide direction depends on anchor mode
    const bool verticalSlide = m_AnchorMode == AnchorMode::TopEdge;
    const int slideDirection = (m_AnchorMode == AnchorMode::LeftEdge || verticalSlide) ? -1 : 1;
    const int slideDistance = 40;
    const int targetCoordinate = verticalSlide ? m_TargetPosition.y() : m_TargetPosition.x();
    const int startCoordinate = targetCoordinate + slideDistance * slideDirection;
    m_SlideAnim->setPropertyName(verticalSlide ? QByteArrayLiteral("y") : QByteArrayLiteral("x"));
    if (verticalSlide) {
        setY(startCoordinate);
    }
    else {
        setX(startCoordinate);
    }
    setOpacity(0.0);

    show();
    raise();

    // Animate slide
    m_SlideAnim->setDuration(220);
    m_SlideAnim->setStartValue(startCoordinate);
    m_SlideAnim->setEndValue(targetCoordinate);
    m_SlideAnim->setEasingCurve(QEasingCurve::OutCubic);

    // Animate opacity: 0 → 1
    m_OpacityAnim->setDuration(220);
    m_OpacityAnim->setStartValue(0.0);
    m_OpacityAnim->setEndValue(1.0);
    m_OpacityAnim->setEasingCurve(QEasingCurve::OutCubic);

    m_SlideAnim->start();
    m_OpacityAnim->start();

    if (m_CloseWhenPointerOutside) {
        schedulePointerOutsideCheck();
    }

    forceRepaint();
}

void OverlayMenuPanel::schedulePointerOutsideCheck()
{
    if (!m_Visible || !m_CloseWhenPointerOutside) {
        return;
    }

    const qint64 remainingGrace = PointerGracePeriodMs - m_ShowTimer.elapsed();
    if (remainingGrace > 0) {
        m_LeaveTimer.start(static_cast<int>(remainingGrace));
    }
    else {
        m_LeaveTimer.start(PointerCheckIntervalMs);
    }
}

void OverlayMenuPanel::repositionWindow()
{
    int qpX = m_ParentX;
    int qpY = m_ParentY;
    int qpW = m_ParentW;
    int qpH = m_ParentH;

    int itemCount  = (int)m_MenuLevels[m_CurrentLevel].items.size();
    int titleH     = m_TitleHeight;
    int menuHeight = titleH + itemCount * m_ItemHeight + m_Padding * 2;

    const QPoint triggerPosition = m_TriggerPosition.value_or(QPoint());

    int cx, cy; // content top-left position

    switch (m_AnchorMode) {
    case AnchorMode::LeftEdge:
        cx = qpX;
        if (m_TriggerPosition.has_value()) {
            cy = triggerPosition.y() - m_TitleHeight - m_Padding - m_ItemHeight / 2;
        }
        else {
            cy = qpY + (qpH - menuHeight) / 2;
        }
        break;

    case AnchorMode::TopEdge:
        if (m_TriggerPosition.has_value()) {
            cx = triggerPosition.x() - m_MenuWidth / 2;
        }
        else {
            cx = qpX + (qpW - m_MenuWidth) / 2;
        }
        cy = qpY;
        break;

    case AnchorMode::AtCursor: {
        // Position menu so cursor is near top-left corner
        cx = triggerPosition.x();
        cy = triggerPosition.y();
        break;
    }

    case AnchorMode::RightEdge:
    default:
        cx = qpX + qpW - m_MenuWidth;
        if (m_TriggerPosition.has_value()) {
            cy = triggerPosition.y() - m_TitleHeight - m_Padding - m_ItemHeight / 2;
        }
        else {
            cy = qpY + (qpH - menuHeight) / 2;
        }
        break;
    }

    // Clamp within the parent. If the menu is larger than the streaming
    // window, keep its origin visible instead of pushing it past an edge.
    cx = qpW >= m_MenuWidth
            ? qBound(qpX, cx, qpX + qpW - m_MenuWidth)
            : qpX;
    cy = qpH >= menuHeight
            ? qBound(qpY, cy, qpY + qpH - menuHeight)
            : qpY;

    // Window includes shadow margin around content
    setGeometry(cx - m_ShadowMargin, cy - m_ShadowMargin,
                m_MenuWidth + 2 * m_ShadowMargin, menuHeight + 2 * m_ShadowMargin);
}

void OverlayMenuPanel::navigateToLevel(int level)
{
    if (level < 0 || level >= (int)m_MenuLevels.size()) return;

    m_LeaveTimer.stop();
    bool goingForward = level > m_CurrentLevel;
    m_ContentSlideAnim->stop();
    m_ContentOffset = 0;

    m_CurrentLevel = level;
    m_HoveredIndex = -1;
    repositionWindow();

    // Reset grace period so Leave event won't close the menu immediately
    // (the mouse may be outside the resized window after navigation)
    m_ShowTimer.start();

    if (m_CloseWhenPointerOutside) {
        schedulePointerOutsideCheck();
    }

    if (goingForward) {
        // Forward: content slides in from right
        m_ContentSlideAnim->setStartValue(30.0);
        m_ContentSlideAnim->setEndValue(0.0);
        m_ContentSlideAnim->start();
    } else {
        // Back: instant switch, no animation (avoids jarring resize + slide combo)
        forceRepaint();
    }
}

void OverlayMenuPanel::closeMenu()
{
    m_LeaveTimer.stop();
    if (!m_Visible) return;
    if (m_Closing) return;  // already animating close

    m_Visible = false;
    m_Closing = true;
    m_HoveredIndex = -1;

    // Stop any show/level animations
    m_SlideAnim->stop();
    m_OpacityAnim->stop();
    m_ContentSlideAnim->stop();
    m_ContentOffset = 0;

    // Animate away from the edge that opened the menu.
    const bool verticalSlide = m_AnchorMode == AnchorMode::TopEdge;
    const int slideDirection = (m_AnchorMode == AnchorMode::LeftEdge || verticalSlide) ? -1 : 1;
    const int slideDistance = 30;
    const int startCoordinate = verticalSlide ? y() : x();
    m_SlideAnim->setPropertyName(verticalSlide ? QByteArrayLiteral("y") : QByteArrayLiteral("x"));
    m_SlideAnim->setDuration(160);
    m_SlideAnim->setStartValue(startCoordinate);
    m_SlideAnim->setEndValue(startCoordinate + slideDistance * slideDirection);
    m_SlideAnim->setEasingCurve(QEasingCurve::InCubic);

    // Animate opacity: current → 0
    m_OpacityAnim->setDuration(160);
    m_OpacityAnim->setStartValue(opacity());
    m_OpacityAnim->setEndValue(0.0);
    m_OpacityAnim->setEasingCurve(QEasingCurve::InCubic);

    // When fade-out completes, finalize (use disconnect to emulate single-shot for Qt 5 compat)
    auto conn = std::make_shared<QMetaObject::Connection>();
    *conn = connect(m_OpacityAnim, &QPropertyAnimation::finished, this, [this, conn]() {
        disconnect(*conn);
        m_Closing = false;
        m_CurrentLevel = 0;
        hide();
        setOpacity(1.0);   // reset for next show
        if (m_CloseCallback) {
            m_CloseCallback();
        }
    });

    m_SlideAnim->start();
    m_OpacityAnim->start();
}

int OverlayMenuPanel::itemAtPos(const QPoint& pos) const
{
    // Adjust for shadow margin
    int lx = pos.x() - m_ShadowMargin;
    int ly = pos.y() - m_ShadowMargin;
    if (lx < 0 || lx >= m_MenuWidth || ly < 0) return -1;

    int titleH = m_TitleHeight;

    if (ly < titleH) {
        if (lx >= m_MenuWidth - m_TitleHeight) {
            return -3; // close button
        }
        if (m_CurrentLevel > 0) {
            return -2; // back button
        }
        return -1;
    }
    int localY = ly - titleH - m_Padding;
    if (localY < 0) return -1;
    int idx = localY / m_ItemHeight;
    const auto& items = m_MenuLevels[m_CurrentLevel].items;
    if (idx < 0 || idx >= (int)items.size()) return -1;
    return idx;
}

// ---------------------------------------------------------------------------
// Painting
// ---------------------------------------------------------------------------

void OverlayMenuPanel::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    int w = width();
    int h = height();
    int sm = m_ShadowMargin;
    int cw = w - 2 * sm;   // content width
    int ch = h - 2 * sm;   // content height

    // Clear to transparent
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(0, 0, w, h, Qt::transparent);
    p.setCompositionMode(QPainter::CompositionMode_SourceOver);

    // === Soft drop shadow ===
    for (int i = sm; i >= 1; i--) {
        qreal t = 1.0 - (qreal)i / sm;
        int alpha = qRound(28.0 * t * t);
        QPainterPath sp;
        sp.addRoundedRect(QRectF(sm - i, sm - i + 1, cw + 2 * i, ch + 2 * i),
                          m_BorderRadius + i, m_BorderRadius + i);
        p.fillPath(sp, QColor(0, 0, 0, alpha));
    }

    // Move to content area
    p.save();
    p.translate(sm, sm);

    // === Win11 dark background ===
    QPainterPath bgPath;
    bgPath.addRoundedRect(QRectF(0, 0, cw, ch), m_BorderRadius, m_BorderRadius);
    p.fillPath(bgPath, QColor(44, 44, 44, 242));

    // Subtle border (Win11 style: thin light outline)
    p.setPen(QPen(QColor(255, 255, 255, 20), 1.0));
    p.drawPath(bgPath);

    // Clip content
    p.setClipPath(bgPath);

    // --- Title bar: back navigation on sub-levels and close on every level ---
    const auto& level = m_MenuLevels[m_CurrentLevel];
    int textPad = (m_CurrentLevel == 0) ? 16 : 8;
    int titleH = m_TitleHeight;
    const bool backHovered = m_CurrentLevel > 0 && m_HoveredIndex == -2;
    const bool closeHovered = m_HoveredIndex == -3;

    if (backHovered) {
        QPainterPath hlPath;
        hlPath.addRoundedRect(QRectF(4, 2, cw - m_TitleHeight - 4,
                                     m_TitleHeight - 4), 4, 4);
        p.fillPath(hlPath, QColor(255, 255, 255, 15));
    }

    const QRect closeRect(cw - m_TitleHeight, 0, m_TitleHeight, m_TitleHeight);
    if (closeHovered) {
        p.fillRect(closeRect, QColor(196, 43, 28, 220));
    }

    p.setFont(m_TitleFont);
    p.setPen(backHovered ? QColor(255, 255, 255, 230) : QColor(255, 255, 255, 160));
    QRect titleRect(textPad, 0, cw - textPad - m_TitleHeight, m_TitleHeight);
    const QString titleText = m_CurrentLevel > 0
            ? QString::fromUtf8("\xe2\x97\x82 ") + level.title
            : level.title;
    p.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, titleText);

    QFont closeFont = m_LabelFont;
    closeFont.setPointSize(11);
    p.setFont(closeFont);
    p.setPen(QColor(255, 255, 255, closeHovered ? 255 : 170));
    p.drawText(closeRect, Qt::AlignCenter, QString::fromUtf8("\xc3\x97"));

    // Apply content offset for level navigation animation
    if (m_ContentSlideAnim->state() != QAbstractAnimation::Running) {
        m_ContentOffset = 0;
    }
    p.save();
    if (m_ContentOffset != 0) {
        p.translate(m_ContentOffset, 0);
    }

    const auto& items = level.items;
    int contentTop = titleH + m_Padding;

    // Icon mapping for menu items
    // Windows: Segoe MDL2 Assets code points
    // Other platforms: Material Icons code points (bundled font)
    auto iconForItem = [](const MenuItem& item) -> QChar {
#ifdef Q_OS_WIN
        // Segoe MDL2 Assets code points
        if (item.type == MenuItemType::SubMenu) {
            if (item.targetLevel == 1) return QChar(0xE713); // Settings gear
            if (item.targetLevel == 2) return QChar(0xE7F4); // DataSense (data/speed)
            if (item.targetLevel == 3) return QChar(0xE707); // Map pin
        }
        switch (item.action) {
        case MenuAction::ToggleFullScreen:  return QChar(0xE740); // FullScreen
        case MenuAction::ShowHostFiles:     return QChar(0xE8B7); // Folder
        case MenuAction::ToggleMicrophone:  return QChar(0xE720); // Microphone
        case MenuAction::ToggleGamepadMouse:  return QChar(0xE7FC); // Gamepad
        case MenuAction::Quit:              return QChar(0xE711); // Close/X
        case MenuAction::QuitAndExit:       return QChar(0xE711); // Close/X
        case MenuAction::ToggleStatsOverlay:return QChar(0xE7F4); // DataSense
        case MenuAction::ToggleMouseMode:   return QChar(0xE962); // Handwriting/pointer
        case MenuAction::ToggleCursorHide:  return QChar(0xE76C); // PointerHand
        case MenuAction::ToggleMinimize:    return QChar(0xE921); // Minimize
        case MenuAction::UngrabInput:       return QChar(0xE785); // Mouse back
        case MenuAction::PasteText:         return QChar(0xE77F); // Paste
        case MenuAction::TogglePointerRegionLock: return QChar(0xE72E); // Lock
        default: return QChar();
        }
#else
        // Material Icons code points
        if (item.type == MenuItemType::SubMenu) {
            if (item.targetLevel == 1) return QChar(0xE8B8); // settings
            if (item.targetLevel == 2) return QChar(0xE1B2); // speed (bitrate)
            if (item.targetLevel == 3) return QChar(0xE55F); // place
        }
        switch (item.action) {
        case MenuAction::ToggleFullScreen:  return QChar(0xE5D0); // fullscreen
        case MenuAction::ShowHostFiles:     return QChar(0xE2C7); // folder
        case MenuAction::ToggleMicrophone:  return QChar(0xE029); // mic
        case MenuAction::ToggleGamepadMouse:  return QChar(0xE30F); // games (gamepad)
        case MenuAction::Quit:              return QChar(0xE5CD); // close
        case MenuAction::QuitAndExit:       return QChar(0xE5CD); // close
        case MenuAction::ToggleStatsOverlay:return QChar(0xE1B2); // speed
        case MenuAction::ToggleMouseMode:   return QChar(0xE323); // mouse (Material)
        case MenuAction::ToggleCursorHide:  return QChar(0xE31A); // near_me (cursor arrow)
        case MenuAction::ToggleMinimize:    return QChar(0xE15B); // remove (minimize bar)
        case MenuAction::UngrabInput:       return QChar(0xE5C4); // arrow_back
        case MenuAction::PasteText:         return QChar(0xE14F); // content_paste
        case MenuAction::TogglePointerRegionLock: return QChar(0xE897); // lock
        default: return QChar();
        }
#endif
    };

    // Icon column: only on top-level menu
    bool hasIcons = (m_CurrentLevel == 0);
    int iconW = hasIcons ? m_IconAreaWidth : 0;
    int labelX = textPad + iconW;

    for (int i = 0; i < (int)items.size(); i++) {
        int itemY = contentTop + i * m_ItemHeight;
        const auto& item = items[i];

        // Hover highlight — Win11 style: subtle rounded rect
        if (i == m_HoveredIndex && item.enabled) {
            QPainterPath hlPath;
            hlPath.addRoundedRect(QRectF(4, itemY + 1, cw - 8, m_ItemHeight - 2), 4, 4);
            p.fillPath(hlPath, QColor(255, 255, 255, 20));
        }

        // Icon (drawn in left area if this level has icons)
        if (hasIcons) {
            QChar icon = iconForItem(item);
            if (!icon.isNull()) {
                p.setFont(m_IconFont);
                p.setPen(item.enabled ? QColor(255, 255, 255, 180) : QColor(255, 255, 255, 60));
                QRect iconRect(textPad, itemY, m_IconAreaWidth, m_ItemHeight);
                p.drawText(iconRect, Qt::AlignCenter, QString(icon));
            }
        }

        // --- SubMenu item ---
        if (item.type == MenuItemType::SubMenu) {
            p.setFont(m_LabelFont);
            p.setPen(item.enabled ? QColor(255, 255, 255, 230) : QColor(255, 255, 255, 80));
            QRect lr(labelX, itemY, cw - labelX - 36, m_ItemHeight);
            p.drawText(lr, Qt::AlignLeft | Qt::AlignVCenter, item.label);

            // Detail text (e.g., "20 Mbps")
            if (!item.detail.isEmpty()) {
                p.setFont(m_DetailFont);
                p.setPen(QColor(255, 255, 255, 100));
                QRect dr(cw / 2, itemY, cw / 2 - textPad - 20, m_ItemHeight);
                p.drawText(dr, Qt::AlignRight | Qt::AlignVCenter, item.detail);
            }

            // Chevron ›
            p.setFont(m_LabelFont);
            p.setPen(QColor(255, 255, 255, 100));
            QRect ar(cw - textPad - 10, itemY, 10, m_ItemHeight);
            p.drawText(ar, Qt::AlignCenter, QString::fromUtf8("\xe2\x80\xba"));
        }
        // --- Toggle item ---
        else if (item.type == MenuItemType::Toggle) {
            p.setFont(m_LabelFont);
            p.setPen(item.enabled ? QColor(255, 255, 255, 230) : QColor(255, 255, 255, 80));
            QRect lr(labelX, itemY, cw - labelX - 52, m_ItemHeight);
            p.drawText(lr, Qt::AlignLeft | Qt::AlignVCenter, item.label);

            // Win11-style toggle switch
            int trackW = 40, trackH = 20;
            int trackX = cw - textPad - trackW;
            int trackY = itemY + (m_ItemHeight - trackH) / 2;

            QPainterPath trackPath;
            trackPath.addRoundedRect(QRectF(trackX, trackY, trackW, trackH),
                                     trackH / 2, trackH / 2);

            int knobR = 6;
            if (item.toggleState) {
                // On: accent fill (Win11 system accent blue)
                p.fillPath(trackPath, QColor(110, 192, 232));
                p.setPen(QPen(QColor(110, 192, 232), 1));
                p.drawPath(trackPath);
                p.setBrush(Qt::white);
                p.setPen(Qt::NoPen);
                p.drawEllipse(QPoint(trackX + trackW - trackH / 2,
                                     trackY + trackH / 2), knobR, knobR);
            } else {
                // Off: transparent with white border
                p.fillPath(trackPath, QColor(255, 255, 255, 0));
                p.setPen(QPen(QColor(255, 255, 255, 120), 1.5));
                p.drawPath(trackPath);
                p.setBrush(QColor(255, 255, 255, 160));
                p.setPen(Qt::NoPen);
                p.drawEllipse(QPoint(trackX + trackH / 2,
                                     trackY + trackH / 2), knobR - 1, knobR - 1);
            }
        }
        // --- Action item ---
        else if (item.type == MenuItemType::Action) {
            p.setFont(m_LabelFont);
            p.setPen(item.enabled ? QColor(255, 255, 255, 230) : QColor(255, 255, 255, 80));

            bool hasLongDetail = !item.detail.isEmpty() && item.detail.length() > 3;
            bool hasShortDetail = !item.detail.isEmpty() && item.detail.length() <= 3;

            if (hasLongDetail) {
                int topH = qRound(m_ItemHeight * 0.58);
                QRect lb(labelX, itemY, cw - labelX - textPad, topH);
                p.drawText(lb, Qt::AlignLeft | Qt::AlignBottom, item.label);

                p.setFont(m_DetailFont);
                p.setPen(QColor(255, 255, 255, 90));
                QRect sr(labelX, itemY + topH, cw - labelX - textPad, m_ItemHeight - topH);
                p.drawText(sr, Qt::AlignLeft | Qt::AlignTop, item.detail);
            } else {
                int detailWidth = 0;
                if (hasShortDetail) {
                    const QFontMetrics detailMetrics(m_DetailFont);
                    detailWidth = qMax(20, detailMetrics.horizontalAdvance(item.detail) + 8);
                }

                const int detailGap = hasShortDetail ? 8 : 0;
                QRect lr(labelX, itemY,
                         cw - labelX - textPad - detailWidth - detailGap,
                         m_ItemHeight);
                p.drawText(lr, Qt::AlignLeft | Qt::AlignVCenter, item.label);

                if (hasShortDetail) {
                    // Short status text or checkmark — Win11 accent color
                    p.setFont(m_DetailFont);
                    p.setPen(QColor(110, 192, 232));
                    QRect cr(cw - textPad - detailWidth, itemY,
                             detailWidth, m_ItemHeight);
                    p.drawText(cr, Qt::AlignRight | Qt::AlignVCenter, item.detail);
                }
            }
        }
        // --- Back item (fallback, normally handled by title bar) ---
        else if (item.type == MenuItemType::Back) {
            p.setFont(m_DetailFont);
            p.setPen(QColor(255, 255, 255, 120));
            QRect lr(labelX, itemY, cw - labelX - textPad, m_ItemHeight);
            p.drawText(lr, Qt::AlignLeft | Qt::AlignVCenter, item.label);
        }

        // Group separator — only where explicitly flagged
        if (item.separatorAfter && i < (int)items.size() - 1) {
            p.setPen(QPen(QColor(255, 255, 255, 18), 1));
            int sepY = itemY + m_ItemHeight - 1;
            p.drawLine(labelX, sepY, cw - textPad, sepY);
        }
    }

    p.restore();  // content offset
    p.restore();  // shadow margin translate
}

// ---------------------------------------------------------------------------
// Mouse input
// ---------------------------------------------------------------------------

void OverlayMenuPanel::mouseMoveEvent(QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    int newIdx = itemAtPos(event->position().toPoint());
#else
    int newIdx = itemAtPos(event->pos());
#endif
    if (newIdx != m_HoveredIndex) {
        m_HoveredIndex = newIdx;
        setCursor((m_HoveredIndex >= 0 || m_HoveredIndex == -2 || m_HoveredIndex == -3)
                          ? Qt::PointingHandCursor
                          : Qt::ArrowCursor);
        forceRepaint();
    }
}

void OverlayMenuPanel::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) return;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    int idx = itemAtPos(event->position().toPoint());
#else
    int idx = itemAtPos(event->pos());
#endif

    if (idx == -3) {
        closeMenu();
        return;
    }

    // Title bar click → navigate back
    if (idx == -2) {
        navigateToLevel(0);
        return;
    }

    if (idx < 0) return;

    const auto& items = m_MenuLevels[m_CurrentLevel].items;
    if (idx >= (int)items.size() || !items[idx].enabled) return;

    const auto& item = items[idx];

    switch (item.type) {
    case MenuItemType::Back:
        navigateToLevel(0);
        break;

    case MenuItemType::SubMenu:
        navigateToLevel(item.targetLevel);
        break;

    case MenuItemType::Action:
    {
        MenuAction action = item.action;
        closeMenu();
        if (m_ActionCallback) {
            m_ActionCallback(action);
        }
        break;
    }

    case MenuItemType::Toggle:
    {
        // Toggle visual state and dispatch
        auto& mutableItem = m_MenuLevels[m_CurrentLevel].items[idx];
        mutableItem.toggleState = !mutableItem.toggleState;
        forceRepaint();
        if (m_ActionCallback) {
            m_ActionCallback(item.action);
        }
        break;
    }
    }
}

// ---------------------------------------------------------------------------
// Gamepad navigation
// ---------------------------------------------------------------------------

void OverlayMenuPanel::gamepadMoveUp()
{
    if (!m_Visible) return;
    const auto& items = m_MenuLevels[m_CurrentLevel].items;
    if (items.empty()) return;

    if (m_HoveredIndex <= 0) {
        // Wrap to last item, or move to title bar if on sub-level
        if (m_CurrentLevel > 0 && m_HoveredIndex == 0) {
            m_HoveredIndex = -2; // title bar (back button)
        } else {
            m_HoveredIndex = (int)items.size() - 1;
        }
    } else {
        m_HoveredIndex--;
    }
    // Skip disabled items
    if (m_HoveredIndex >= 0 && !items[m_HoveredIndex].enabled) {
        gamepadMoveUp();
        return;
    }
    forceRepaint();
}

void OverlayMenuPanel::gamepadMoveDown()
{
    if (!m_Visible) return;
    const auto& items = m_MenuLevels[m_CurrentLevel].items;
    if (items.empty()) return;

    if (m_HoveredIndex == -2) {
        // From title bar, move to first item
        m_HoveredIndex = 0;
    } else if (m_HoveredIndex < 0 || m_HoveredIndex >= (int)items.size() - 1) {
        // Wrap to title bar on sub-level, or to first item on top level
        if (m_CurrentLevel > 0) {
            m_HoveredIndex = -2;
        } else {
            m_HoveredIndex = 0;
        }
    } else {
        m_HoveredIndex++;
    }
    // Skip disabled items
    if (m_HoveredIndex >= 0 && !items[m_HoveredIndex].enabled) {
        gamepadMoveDown();
        return;
    }
    forceRepaint();
}

void OverlayMenuPanel::gamepadSelect()
{
    if (!m_Visible) return;

    // Title bar → back
    if (m_HoveredIndex == -2) {
        navigateToLevel(0);
        return;
    }

    if (m_HoveredIndex < 0) return;

    const auto& items = m_MenuLevels[m_CurrentLevel].items;
    if (m_HoveredIndex >= (int)items.size() || !items[m_HoveredIndex].enabled) return;

    const auto& item = items[m_HoveredIndex];

    switch (item.type) {
    case MenuItemType::Back:
        navigateToLevel(0);
        break;
    case MenuItemType::SubMenu:
        navigateToLevel(item.targetLevel);
        break;
    case MenuItemType::Action:
    {
        MenuAction action = item.action;
        closeMenu();
        if (m_ActionCallback) {
            m_ActionCallback(action);
        }
        break;
    }
    case MenuItemType::Toggle:
    {
        auto& mutableItem = m_MenuLevels[m_CurrentLevel].items[m_HoveredIndex];
        mutableItem.toggleState = !mutableItem.toggleState;
        forceRepaint();
        if (m_ActionCallback) {
            m_ActionCallback(item.action);
        }
        break;
    }
    }
}

void OverlayMenuPanel::gamepadBack()
{
    if (!m_Visible) return;

    if (m_CurrentLevel > 0) {
        navigateToLevel(0);
    } else {
        closeMenu();
    }
}

bool OverlayMenuPanel::event(QEvent* ev)
{
    if (ev->type() == QEvent::Leave) {
        if (m_Visible && m_CloseWhenPointerOutside) {
            // During the grace period, defer the outside check instead of
            // dropping the Leave event. Otherwise, leaving the panel quickly
            // after it opens would keep it visible until the cursor entered
            // and left the panel again.
            schedulePointerOutsideCheck();
        }
        return true;
    }
    return QRasterWindow::event(ev);
}
