/****************************************************************************
** Meta object code from reading C++ file 'streamingpreferences.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.9.1)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../settings/streamingpreferences.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'streamingpreferences.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.9.1. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN20StreamingPreferencesE_t {};
} // unnamed namespace

template <> constexpr inline auto StreamingPreferences::qt_create_metaobjectdata<qt_meta_tag_ZN20StreamingPreferencesE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "StreamingPreferences",
        "displayModeChanged",
        "",
        "bitrateChanged",
        "unlockBitrateChanged",
        "autoAdjustBitrateChanged",
        "enableVsyncChanged",
        "gameOptimizationsChanged",
        "playAudioOnHostChanged",
        "multiControllerChanged",
        "unsupportedFpsChanged",
        "enableMdnsChanged",
        "quitAppAfterChanged",
        "absoluteMouseModeChanged",
        "absoluteTouchModeChanged",
        "audioConfigChanged",
        "videoCodecConfigChanged",
        "enableHdrChanged",
        "enableYUV444Changed",
        "videoDecoderSelectionChanged",
        "uiDisplayModeChanged",
        "windowModeChanged",
        "framePacingChanged",
        "connectionWarningsChanged",
        "configurationWarningsChanged",
        "richPresenceChanged",
        "gamepadMouseChanged",
        "detectNetworkBlockingChanged",
        "showPerformanceOverlayChanged",
        "mouseButtonsChanged",
        "muteOnFocusLossChanged",
        "backgroundGamepadChanged",
        "reverseScrollDirectionChanged",
        "swapFaceButtonsChanged",
        "captureSysKeysModeChanged",
        "keepAwakeChanged",
        "languageChanged",
        "getDefaultBitrate",
        "width",
        "height",
        "fps",
        "yuv444",
        "save",
        "retranslate",
        "bitrateKbps",
        "unlockBitrate",
        "autoAdjustBitrate",
        "enableVsync",
        "gameOptimizations",
        "playAudioOnHost",
        "multiController",
        "enableMdns",
        "quitAppAfter",
        "absoluteMouseMode",
        "absoluteTouchMode",
        "framePacing",
        "connectionWarnings",
        "configurationWarnings",
        "richPresence",
        "gamepadMouse",
        "detectNetworkBlocking",
        "showPerformanceOverlay",
        "audioConfig",
        "AudioConfig",
        "videoCodecConfig",
        "VideoCodecConfig",
        "enableHdr",
        "enableYUV444",
        "videoDecoderSelection",
        "VideoDecoderSelection",
        "windowMode",
        "WindowMode",
        "recommendedFullScreenMode",
        "uiDisplayMode",
        "UIDisplayMode",
        "swapMouseButtons",
        "muteOnFocusLoss",
        "backgroundGamepad",
        "reverseScrollDirection",
        "swapFaceButtons",
        "keepAwake",
        "captureSysKeysMode",
        "CaptureSysKeysMode",
        "language",
        "Language",
        "AC_STEREO",
        "AC_51_SURROUND",
        "AC_71_SURROUND",
        "VCC_AUTO",
        "VCC_FORCE_H264",
        "VCC_FORCE_HEVC",
        "VCC_FORCE_HEVC_HDR_DEPRECATED",
        "VCC_FORCE_AV1",
        "VDS_AUTO",
        "VDS_FORCE_HARDWARE",
        "VDS_FORCE_SOFTWARE",
        "WM_FULLSCREEN",
        "WM_FULLSCREEN_DESKTOP",
        "WM_WINDOWED",
        "UI_WINDOWED",
        "UI_MAXIMIZED",
        "UI_FULLSCREEN",
        "LANG_AUTO",
        "LANG_EN",
        "LANG_FR",
        "LANG_ZH_CN",
        "LANG_DE",
        "LANG_NB_NO",
        "LANG_RU",
        "LANG_ES",
        "LANG_JA",
        "LANG_VI",
        "LANG_TH",
        "LANG_KO",
        "LANG_HU",
        "LANG_NL",
        "LANG_SV",
        "LANG_TR",
        "LANG_UK",
        "LANG_ZH_TW",
        "LANG_PT",
        "LANG_PT_BR",
        "LANG_EL",
        "LANG_IT",
        "LANG_HI",
        "LANG_PL",
        "LANG_CS",
        "LANG_HE",
        "LANG_CKB",
        "LANG_LT",
        "LANG_ET",
        "LANG_BG",
        "LANG_EO",
        "LANG_TA",
        "CSK_OFF",
        "CSK_FULLSCREEN",
        "CSK_ALWAYS"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'displayModeChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'bitrateChanged'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'unlockBitrateChanged'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'autoAdjustBitrateChanged'
        QtMocHelpers::SignalData<void()>(5, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'enableVsyncChanged'
        QtMocHelpers::SignalData<void()>(6, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'gameOptimizationsChanged'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'playAudioOnHostChanged'
        QtMocHelpers::SignalData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'multiControllerChanged'
        QtMocHelpers::SignalData<void()>(9, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'unsupportedFpsChanged'
        QtMocHelpers::SignalData<void()>(10, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'enableMdnsChanged'
        QtMocHelpers::SignalData<void()>(11, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'quitAppAfterChanged'
        QtMocHelpers::SignalData<void()>(12, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'absoluteMouseModeChanged'
        QtMocHelpers::SignalData<void()>(13, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'absoluteTouchModeChanged'
        QtMocHelpers::SignalData<void()>(14, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'audioConfigChanged'
        QtMocHelpers::SignalData<void()>(15, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'videoCodecConfigChanged'
        QtMocHelpers::SignalData<void()>(16, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'enableHdrChanged'
        QtMocHelpers::SignalData<void()>(17, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'enableYUV444Changed'
        QtMocHelpers::SignalData<void()>(18, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'videoDecoderSelectionChanged'
        QtMocHelpers::SignalData<void()>(19, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'uiDisplayModeChanged'
        QtMocHelpers::SignalData<void()>(20, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'windowModeChanged'
        QtMocHelpers::SignalData<void()>(21, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'framePacingChanged'
        QtMocHelpers::SignalData<void()>(22, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'connectionWarningsChanged'
        QtMocHelpers::SignalData<void()>(23, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'configurationWarningsChanged'
        QtMocHelpers::SignalData<void()>(24, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'richPresenceChanged'
        QtMocHelpers::SignalData<void()>(25, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'gamepadMouseChanged'
        QtMocHelpers::SignalData<void()>(26, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'detectNetworkBlockingChanged'
        QtMocHelpers::SignalData<void()>(27, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'showPerformanceOverlayChanged'
        QtMocHelpers::SignalData<void()>(28, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'mouseButtonsChanged'
        QtMocHelpers::SignalData<void()>(29, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'muteOnFocusLossChanged'
        QtMocHelpers::SignalData<void()>(30, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'backgroundGamepadChanged'
        QtMocHelpers::SignalData<void()>(31, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'reverseScrollDirectionChanged'
        QtMocHelpers::SignalData<void()>(32, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'swapFaceButtonsChanged'
        QtMocHelpers::SignalData<void()>(33, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'captureSysKeysModeChanged'
        QtMocHelpers::SignalData<void()>(34, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'keepAwakeChanged'
        QtMocHelpers::SignalData<void()>(35, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'languageChanged'
        QtMocHelpers::SignalData<void()>(36, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'getDefaultBitrate'
        QtMocHelpers::MethodData<int(int, int, int, bool)>(37, 2, QMC::AccessPublic, QMetaType::Int, {{
            { QMetaType::Int, 38 }, { QMetaType::Int, 39 }, { QMetaType::Int, 40 }, { QMetaType::Bool, 41 },
        }}),
        // Method 'save'
        QtMocHelpers::MethodData<void()>(42, 2, QMC::AccessPublic, QMetaType::Void),
        // Method 'retranslate'
        QtMocHelpers::MethodData<bool()>(43, 2, QMC::AccessPublic, QMetaType::Bool),
    };
    QtMocHelpers::UintData qt_properties {
        // property 'width'
        QtMocHelpers::PropertyData<int>(38, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Writable, 0),
        // property 'height'
        QtMocHelpers::PropertyData<int>(39, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Writable, 0),
        // property 'fps'
        QtMocHelpers::PropertyData<int>(40, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Writable, 0),
        // property 'bitrateKbps'
        QtMocHelpers::PropertyData<int>(44, QMetaType::Int, QMC::DefaultPropertyFlags | QMC::Writable, 1),
        // property 'unlockBitrate'
        QtMocHelpers::PropertyData<bool>(45, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 2),
        // property 'autoAdjustBitrate'
        QtMocHelpers::PropertyData<bool>(46, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 3),
        // property 'enableVsync'
        QtMocHelpers::PropertyData<bool>(47, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 4),
        // property 'gameOptimizations'
        QtMocHelpers::PropertyData<bool>(48, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 5),
        // property 'playAudioOnHost'
        QtMocHelpers::PropertyData<bool>(49, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 6),
        // property 'multiController'
        QtMocHelpers::PropertyData<bool>(50, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 7),
        // property 'enableMdns'
        QtMocHelpers::PropertyData<bool>(51, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 9),
        // property 'quitAppAfter'
        QtMocHelpers::PropertyData<bool>(52, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 10),
        // property 'absoluteMouseMode'
        QtMocHelpers::PropertyData<bool>(53, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 11),
        // property 'absoluteTouchMode'
        QtMocHelpers::PropertyData<bool>(54, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 12),
        // property 'framePacing'
        QtMocHelpers::PropertyData<bool>(55, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 20),
        // property 'connectionWarnings'
        QtMocHelpers::PropertyData<bool>(56, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 21),
        // property 'configurationWarnings'
        QtMocHelpers::PropertyData<bool>(57, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 22),
        // property 'richPresence'
        QtMocHelpers::PropertyData<bool>(58, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 23),
        // property 'gamepadMouse'
        QtMocHelpers::PropertyData<bool>(59, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 24),
        // property 'detectNetworkBlocking'
        QtMocHelpers::PropertyData<bool>(60, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 25),
        // property 'showPerformanceOverlay'
        QtMocHelpers::PropertyData<bool>(61, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 26),
        // property 'audioConfig'
        QtMocHelpers::PropertyData<AudioConfig>(62, 0x80000000 | 63, QMC::DefaultPropertyFlags | QMC::Writable | QMC::EnumOrFlag, 13),
        // property 'videoCodecConfig'
        QtMocHelpers::PropertyData<VideoCodecConfig>(64, 0x80000000 | 65, QMC::DefaultPropertyFlags | QMC::Writable | QMC::EnumOrFlag, 14),
        // property 'enableHdr'
        QtMocHelpers::PropertyData<bool>(66, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 15),
        // property 'enableYUV444'
        QtMocHelpers::PropertyData<bool>(67, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 16),
        // property 'videoDecoderSelection'
        QtMocHelpers::PropertyData<VideoDecoderSelection>(68, 0x80000000 | 69, QMC::DefaultPropertyFlags | QMC::Writable | QMC::EnumOrFlag, 17),
        // property 'windowMode'
        QtMocHelpers::PropertyData<WindowMode>(70, 0x80000000 | 71, QMC::DefaultPropertyFlags | QMC::Writable | QMC::EnumOrFlag, 19),
        // property 'recommendedFullScreenMode'
        QtMocHelpers::PropertyData<WindowMode>(72, 0x80000000 | 71, QMC::DefaultPropertyFlags | QMC::EnumOrFlag | QMC::Constant),
        // property 'uiDisplayMode'
        QtMocHelpers::PropertyData<UIDisplayMode>(73, 0x80000000 | 74, QMC::DefaultPropertyFlags | QMC::Writable | QMC::EnumOrFlag, 18),
        // property 'swapMouseButtons'
        QtMocHelpers::PropertyData<bool>(75, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 27),
        // property 'muteOnFocusLoss'
        QtMocHelpers::PropertyData<bool>(76, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 28),
        // property 'backgroundGamepad'
        QtMocHelpers::PropertyData<bool>(77, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 29),
        // property 'reverseScrollDirection'
        QtMocHelpers::PropertyData<bool>(78, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 30),
        // property 'swapFaceButtons'
        QtMocHelpers::PropertyData<bool>(79, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 31),
        // property 'keepAwake'
        QtMocHelpers::PropertyData<bool>(80, QMetaType::Bool, QMC::DefaultPropertyFlags | QMC::Writable, 33),
        // property 'captureSysKeysMode'
        QtMocHelpers::PropertyData<CaptureSysKeysMode>(81, 0x80000000 | 82, QMC::DefaultPropertyFlags | QMC::Writable | QMC::EnumOrFlag, 32),
        // property 'language'
        QtMocHelpers::PropertyData<Language>(83, 0x80000000 | 84, QMC::DefaultPropertyFlags | QMC::Writable | QMC::EnumOrFlag, 34),
    };
    QtMocHelpers::UintData qt_enums {
        // enum 'AudioConfig'
        QtMocHelpers::EnumData<AudioConfig>(63, 63, QMC::EnumFlags{}).add({
            {   85, AudioConfig::AC_STEREO },
            {   86, AudioConfig::AC_51_SURROUND },
            {   87, AudioConfig::AC_71_SURROUND },
        }),
        // enum 'VideoCodecConfig'
        QtMocHelpers::EnumData<VideoCodecConfig>(65, 65, QMC::EnumFlags{}).add({
            {   88, VideoCodecConfig::VCC_AUTO },
            {   89, VideoCodecConfig::VCC_FORCE_H264 },
            {   90, VideoCodecConfig::VCC_FORCE_HEVC },
            {   91, VideoCodecConfig::VCC_FORCE_HEVC_HDR_DEPRECATED },
            {   92, VideoCodecConfig::VCC_FORCE_AV1 },
        }),
        // enum 'VideoDecoderSelection'
        QtMocHelpers::EnumData<VideoDecoderSelection>(69, 69, QMC::EnumFlags{}).add({
            {   93, VideoDecoderSelection::VDS_AUTO },
            {   94, VideoDecoderSelection::VDS_FORCE_HARDWARE },
            {   95, VideoDecoderSelection::VDS_FORCE_SOFTWARE },
        }),
        // enum 'WindowMode'
        QtMocHelpers::EnumData<WindowMode>(71, 71, QMC::EnumFlags{}).add({
            {   96, WindowMode::WM_FULLSCREEN },
            {   97, WindowMode::WM_FULLSCREEN_DESKTOP },
            {   98, WindowMode::WM_WINDOWED },
        }),
        // enum 'UIDisplayMode'
        QtMocHelpers::EnumData<UIDisplayMode>(74, 74, QMC::EnumFlags{}).add({
            {   99, UIDisplayMode::UI_WINDOWED },
            {  100, UIDisplayMode::UI_MAXIMIZED },
            {  101, UIDisplayMode::UI_FULLSCREEN },
        }),
        // enum 'Language'
        QtMocHelpers::EnumData<Language>(84, 84, QMC::EnumFlags{}).add({
            {  102, Language::LANG_AUTO },
            {  103, Language::LANG_EN },
            {  104, Language::LANG_FR },
            {  105, Language::LANG_ZH_CN },
            {  106, Language::LANG_DE },
            {  107, Language::LANG_NB_NO },
            {  108, Language::LANG_RU },
            {  109, Language::LANG_ES },
            {  110, Language::LANG_JA },
            {  111, Language::LANG_VI },
            {  112, Language::LANG_TH },
            {  113, Language::LANG_KO },
            {  114, Language::LANG_HU },
            {  115, Language::LANG_NL },
            {  116, Language::LANG_SV },
            {  117, Language::LANG_TR },
            {  118, Language::LANG_UK },
            {  119, Language::LANG_ZH_TW },
            {  120, Language::LANG_PT },
            {  121, Language::LANG_PT_BR },
            {  122, Language::LANG_EL },
            {  123, Language::LANG_IT },
            {  124, Language::LANG_HI },
            {  125, Language::LANG_PL },
            {  126, Language::LANG_CS },
            {  127, Language::LANG_HE },
            {  128, Language::LANG_CKB },
            {  129, Language::LANG_LT },
            {  130, Language::LANG_ET },
            {  131, Language::LANG_BG },
            {  132, Language::LANG_EO },
            {  133, Language::LANG_TA },
        }),
        // enum 'CaptureSysKeysMode'
        QtMocHelpers::EnumData<CaptureSysKeysMode>(82, 82, QMC::EnumFlags{}).add({
            {  134, CaptureSysKeysMode::CSK_OFF },
            {  135, CaptureSysKeysMode::CSK_FULLSCREEN },
            {  136, CaptureSysKeysMode::CSK_ALWAYS },
        }),
    };
    return QtMocHelpers::metaObjectData<StreamingPreferences, qt_meta_tag_ZN20StreamingPreferencesE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject StreamingPreferences::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN20StreamingPreferencesE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN20StreamingPreferencesE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN20StreamingPreferencesE_t>.metaTypes,
    nullptr
} };

void StreamingPreferences::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<StreamingPreferences *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->displayModeChanged(); break;
        case 1: _t->bitrateChanged(); break;
        case 2: _t->unlockBitrateChanged(); break;
        case 3: _t->autoAdjustBitrateChanged(); break;
        case 4: _t->enableVsyncChanged(); break;
        case 5: _t->gameOptimizationsChanged(); break;
        case 6: _t->playAudioOnHostChanged(); break;
        case 7: _t->multiControllerChanged(); break;
        case 8: _t->unsupportedFpsChanged(); break;
        case 9: _t->enableMdnsChanged(); break;
        case 10: _t->quitAppAfterChanged(); break;
        case 11: _t->absoluteMouseModeChanged(); break;
        case 12: _t->absoluteTouchModeChanged(); break;
        case 13: _t->audioConfigChanged(); break;
        case 14: _t->videoCodecConfigChanged(); break;
        case 15: _t->enableHdrChanged(); break;
        case 16: _t->enableYUV444Changed(); break;
        case 17: _t->videoDecoderSelectionChanged(); break;
        case 18: _t->uiDisplayModeChanged(); break;
        case 19: _t->windowModeChanged(); break;
        case 20: _t->framePacingChanged(); break;
        case 21: _t->connectionWarningsChanged(); break;
        case 22: _t->configurationWarningsChanged(); break;
        case 23: _t->richPresenceChanged(); break;
        case 24: _t->gamepadMouseChanged(); break;
        case 25: _t->detectNetworkBlockingChanged(); break;
        case 26: _t->showPerformanceOverlayChanged(); break;
        case 27: _t->mouseButtonsChanged(); break;
        case 28: _t->muteOnFocusLossChanged(); break;
        case 29: _t->backgroundGamepadChanged(); break;
        case 30: _t->reverseScrollDirectionChanged(); break;
        case 31: _t->swapFaceButtonsChanged(); break;
        case 32: _t->captureSysKeysModeChanged(); break;
        case 33: _t->keepAwakeChanged(); break;
        case 34: _t->languageChanged(); break;
        case 35: { int _r = _t->getDefaultBitrate((*reinterpret_cast< std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[2])),(*reinterpret_cast< std::add_pointer_t<int>>(_a[3])),(*reinterpret_cast< std::add_pointer_t<bool>>(_a[4])));
            if (_a[0]) *reinterpret_cast< int*>(_a[0]) = std::move(_r); }  break;
        case 36: _t->save(); break;
        case 37: { bool _r = _t->retranslate();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::displayModeChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::bitrateChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::unlockBitrateChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::autoAdjustBitrateChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::enableVsyncChanged, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::gameOptimizationsChanged, 5))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::playAudioOnHostChanged, 6))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::multiControllerChanged, 7))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::unsupportedFpsChanged, 8))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::enableMdnsChanged, 9))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::quitAppAfterChanged, 10))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::absoluteMouseModeChanged, 11))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::absoluteTouchModeChanged, 12))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::audioConfigChanged, 13))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::videoCodecConfigChanged, 14))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::enableHdrChanged, 15))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::enableYUV444Changed, 16))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::videoDecoderSelectionChanged, 17))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::uiDisplayModeChanged, 18))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::windowModeChanged, 19))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::framePacingChanged, 20))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::connectionWarningsChanged, 21))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::configurationWarningsChanged, 22))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::richPresenceChanged, 23))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::gamepadMouseChanged, 24))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::detectNetworkBlockingChanged, 25))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::showPerformanceOverlayChanged, 26))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::mouseButtonsChanged, 27))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::muteOnFocusLossChanged, 28))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::backgroundGamepadChanged, 29))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::reverseScrollDirectionChanged, 30))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::swapFaceButtonsChanged, 31))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::captureSysKeysModeChanged, 32))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::keepAwakeChanged, 33))
            return;
        if (QtMocHelpers::indexOfMethod<void (StreamingPreferences::*)()>(_a, &StreamingPreferences::languageChanged, 34))
            return;
    }
    if (_c == QMetaObject::ReadProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0: *reinterpret_cast<int*>(_v) = _t->width; break;
        case 1: *reinterpret_cast<int*>(_v) = _t->height; break;
        case 2: *reinterpret_cast<int*>(_v) = _t->fps; break;
        case 3: *reinterpret_cast<int*>(_v) = _t->bitrateKbps; break;
        case 4: *reinterpret_cast<bool*>(_v) = _t->unlockBitrate; break;
        case 5: *reinterpret_cast<bool*>(_v) = _t->autoAdjustBitrate; break;
        case 6: *reinterpret_cast<bool*>(_v) = _t->enableVsync; break;
        case 7: *reinterpret_cast<bool*>(_v) = _t->gameOptimizations; break;
        case 8: *reinterpret_cast<bool*>(_v) = _t->playAudioOnHost; break;
        case 9: *reinterpret_cast<bool*>(_v) = _t->multiController; break;
        case 10: *reinterpret_cast<bool*>(_v) = _t->enableMdns; break;
        case 11: *reinterpret_cast<bool*>(_v) = _t->quitAppAfter; break;
        case 12: *reinterpret_cast<bool*>(_v) = _t->absoluteMouseMode; break;
        case 13: *reinterpret_cast<bool*>(_v) = _t->absoluteTouchMode; break;
        case 14: *reinterpret_cast<bool*>(_v) = _t->framePacing; break;
        case 15: *reinterpret_cast<bool*>(_v) = _t->connectionWarnings; break;
        case 16: *reinterpret_cast<bool*>(_v) = _t->configurationWarnings; break;
        case 17: *reinterpret_cast<bool*>(_v) = _t->richPresence; break;
        case 18: *reinterpret_cast<bool*>(_v) = _t->gamepadMouse; break;
        case 19: *reinterpret_cast<bool*>(_v) = _t->detectNetworkBlocking; break;
        case 20: *reinterpret_cast<bool*>(_v) = _t->showPerformanceOverlay; break;
        case 21: *reinterpret_cast<AudioConfig*>(_v) = _t->audioConfig; break;
        case 22: *reinterpret_cast<VideoCodecConfig*>(_v) = _t->videoCodecConfig; break;
        case 23: *reinterpret_cast<bool*>(_v) = _t->enableHdr; break;
        case 24: *reinterpret_cast<bool*>(_v) = _t->enableYUV444; break;
        case 25: *reinterpret_cast<VideoDecoderSelection*>(_v) = _t->videoDecoderSelection; break;
        case 26: *reinterpret_cast<WindowMode*>(_v) = _t->windowMode; break;
        case 27: *reinterpret_cast<WindowMode*>(_v) = _t->recommendedFullScreenMode; break;
        case 28: *reinterpret_cast<UIDisplayMode*>(_v) = _t->uiDisplayMode; break;
        case 29: *reinterpret_cast<bool*>(_v) = _t->swapMouseButtons; break;
        case 30: *reinterpret_cast<bool*>(_v) = _t->muteOnFocusLoss; break;
        case 31: *reinterpret_cast<bool*>(_v) = _t->backgroundGamepad; break;
        case 32: *reinterpret_cast<bool*>(_v) = _t->reverseScrollDirection; break;
        case 33: *reinterpret_cast<bool*>(_v) = _t->swapFaceButtons; break;
        case 34: *reinterpret_cast<bool*>(_v) = _t->keepAwake; break;
        case 35: *reinterpret_cast<CaptureSysKeysMode*>(_v) = _t->captureSysKeysMode; break;
        case 36: *reinterpret_cast<Language*>(_v) = _t->language; break;
        default: break;
        }
    }
    if (_c == QMetaObject::WriteProperty) {
        void *_v = _a[0];
        switch (_id) {
        case 0:
            if (QtMocHelpers::setProperty(_t->width, *reinterpret_cast<int*>(_v)))
                Q_EMIT _t->displayModeChanged();
            break;
        case 1:
            if (QtMocHelpers::setProperty(_t->height, *reinterpret_cast<int*>(_v)))
                Q_EMIT _t->displayModeChanged();
            break;
        case 2:
            if (QtMocHelpers::setProperty(_t->fps, *reinterpret_cast<int*>(_v)))
                Q_EMIT _t->displayModeChanged();
            break;
        case 3:
            if (QtMocHelpers::setProperty(_t->bitrateKbps, *reinterpret_cast<int*>(_v)))
                Q_EMIT _t->bitrateChanged();
            break;
        case 4:
            if (QtMocHelpers::setProperty(_t->unlockBitrate, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->unlockBitrateChanged();
            break;
        case 5:
            if (QtMocHelpers::setProperty(_t->autoAdjustBitrate, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->autoAdjustBitrateChanged();
            break;
        case 6:
            if (QtMocHelpers::setProperty(_t->enableVsync, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->enableVsyncChanged();
            break;
        case 7:
            if (QtMocHelpers::setProperty(_t->gameOptimizations, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->gameOptimizationsChanged();
            break;
        case 8:
            if (QtMocHelpers::setProperty(_t->playAudioOnHost, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->playAudioOnHostChanged();
            break;
        case 9:
            if (QtMocHelpers::setProperty(_t->multiController, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->multiControllerChanged();
            break;
        case 10:
            if (QtMocHelpers::setProperty(_t->enableMdns, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->enableMdnsChanged();
            break;
        case 11:
            if (QtMocHelpers::setProperty(_t->quitAppAfter, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->quitAppAfterChanged();
            break;
        case 12:
            if (QtMocHelpers::setProperty(_t->absoluteMouseMode, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->absoluteMouseModeChanged();
            break;
        case 13:
            if (QtMocHelpers::setProperty(_t->absoluteTouchMode, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->absoluteTouchModeChanged();
            break;
        case 14:
            if (QtMocHelpers::setProperty(_t->framePacing, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->framePacingChanged();
            break;
        case 15:
            if (QtMocHelpers::setProperty(_t->connectionWarnings, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->connectionWarningsChanged();
            break;
        case 16:
            if (QtMocHelpers::setProperty(_t->configurationWarnings, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->configurationWarningsChanged();
            break;
        case 17:
            if (QtMocHelpers::setProperty(_t->richPresence, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->richPresenceChanged();
            break;
        case 18:
            if (QtMocHelpers::setProperty(_t->gamepadMouse, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->gamepadMouseChanged();
            break;
        case 19:
            if (QtMocHelpers::setProperty(_t->detectNetworkBlocking, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->detectNetworkBlockingChanged();
            break;
        case 20:
            if (QtMocHelpers::setProperty(_t->showPerformanceOverlay, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->showPerformanceOverlayChanged();
            break;
        case 21:
            if (QtMocHelpers::setProperty(_t->audioConfig, *reinterpret_cast<AudioConfig*>(_v)))
                Q_EMIT _t->audioConfigChanged();
            break;
        case 22:
            if (QtMocHelpers::setProperty(_t->videoCodecConfig, *reinterpret_cast<VideoCodecConfig*>(_v)))
                Q_EMIT _t->videoCodecConfigChanged();
            break;
        case 23:
            if (QtMocHelpers::setProperty(_t->enableHdr, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->enableHdrChanged();
            break;
        case 24:
            if (QtMocHelpers::setProperty(_t->enableYUV444, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->enableYUV444Changed();
            break;
        case 25:
            if (QtMocHelpers::setProperty(_t->videoDecoderSelection, *reinterpret_cast<VideoDecoderSelection*>(_v)))
                Q_EMIT _t->videoDecoderSelectionChanged();
            break;
        case 26:
            if (QtMocHelpers::setProperty(_t->windowMode, *reinterpret_cast<WindowMode*>(_v)))
                Q_EMIT _t->windowModeChanged();
            break;
        case 28:
            if (QtMocHelpers::setProperty(_t->uiDisplayMode, *reinterpret_cast<UIDisplayMode*>(_v)))
                Q_EMIT _t->uiDisplayModeChanged();
            break;
        case 29:
            if (QtMocHelpers::setProperty(_t->swapMouseButtons, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->mouseButtonsChanged();
            break;
        case 30:
            if (QtMocHelpers::setProperty(_t->muteOnFocusLoss, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->muteOnFocusLossChanged();
            break;
        case 31:
            if (QtMocHelpers::setProperty(_t->backgroundGamepad, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->backgroundGamepadChanged();
            break;
        case 32:
            if (QtMocHelpers::setProperty(_t->reverseScrollDirection, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->reverseScrollDirectionChanged();
            break;
        case 33:
            if (QtMocHelpers::setProperty(_t->swapFaceButtons, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->swapFaceButtonsChanged();
            break;
        case 34:
            if (QtMocHelpers::setProperty(_t->keepAwake, *reinterpret_cast<bool*>(_v)))
                Q_EMIT _t->keepAwakeChanged();
            break;
        case 35:
            if (QtMocHelpers::setProperty(_t->captureSysKeysMode, *reinterpret_cast<CaptureSysKeysMode*>(_v)))
                Q_EMIT _t->captureSysKeysModeChanged();
            break;
        case 36:
            if (QtMocHelpers::setProperty(_t->language, *reinterpret_cast<Language*>(_v)))
                Q_EMIT _t->languageChanged();
            break;
        default: break;
        }
    }
}

const QMetaObject *StreamingPreferences::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *StreamingPreferences::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN20StreamingPreferencesE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int StreamingPreferences::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 38)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 38;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 38)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 38;
    }
    if (_c == QMetaObject::ReadProperty || _c == QMetaObject::WriteProperty
            || _c == QMetaObject::ResetProperty || _c == QMetaObject::BindableProperty
            || _c == QMetaObject::RegisterPropertyMetaType) {
        qt_static_metacall(this, _c, _id, _a);
        _id -= 37;
    }
    return _id;
}

// SIGNAL 0
void StreamingPreferences::displayModeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void StreamingPreferences::bitrateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void StreamingPreferences::unlockBitrateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void StreamingPreferences::autoAdjustBitrateChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 3, nullptr);
}

// SIGNAL 4
void StreamingPreferences::enableVsyncChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void StreamingPreferences::gameOptimizationsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}

// SIGNAL 6
void StreamingPreferences::playAudioOnHostChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 6, nullptr);
}

// SIGNAL 7
void StreamingPreferences::multiControllerChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 7, nullptr);
}

// SIGNAL 8
void StreamingPreferences::unsupportedFpsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 8, nullptr);
}

// SIGNAL 9
void StreamingPreferences::enableMdnsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 9, nullptr);
}

// SIGNAL 10
void StreamingPreferences::quitAppAfterChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 10, nullptr);
}

// SIGNAL 11
void StreamingPreferences::absoluteMouseModeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 11, nullptr);
}

// SIGNAL 12
void StreamingPreferences::absoluteTouchModeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 12, nullptr);
}

// SIGNAL 13
void StreamingPreferences::audioConfigChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 13, nullptr);
}

// SIGNAL 14
void StreamingPreferences::videoCodecConfigChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 14, nullptr);
}

// SIGNAL 15
void StreamingPreferences::enableHdrChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 15, nullptr);
}

// SIGNAL 16
void StreamingPreferences::enableYUV444Changed()
{
    QMetaObject::activate(this, &staticMetaObject, 16, nullptr);
}

// SIGNAL 17
void StreamingPreferences::videoDecoderSelectionChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 17, nullptr);
}

// SIGNAL 18
void StreamingPreferences::uiDisplayModeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 18, nullptr);
}

// SIGNAL 19
void StreamingPreferences::windowModeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 19, nullptr);
}

// SIGNAL 20
void StreamingPreferences::framePacingChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 20, nullptr);
}

// SIGNAL 21
void StreamingPreferences::connectionWarningsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 21, nullptr);
}

// SIGNAL 22
void StreamingPreferences::configurationWarningsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 22, nullptr);
}

// SIGNAL 23
void StreamingPreferences::richPresenceChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 23, nullptr);
}

// SIGNAL 24
void StreamingPreferences::gamepadMouseChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 24, nullptr);
}

// SIGNAL 25
void StreamingPreferences::detectNetworkBlockingChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 25, nullptr);
}

// SIGNAL 26
void StreamingPreferences::showPerformanceOverlayChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 26, nullptr);
}

// SIGNAL 27
void StreamingPreferences::mouseButtonsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 27, nullptr);
}

// SIGNAL 28
void StreamingPreferences::muteOnFocusLossChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 28, nullptr);
}

// SIGNAL 29
void StreamingPreferences::backgroundGamepadChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 29, nullptr);
}

// SIGNAL 30
void StreamingPreferences::reverseScrollDirectionChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 30, nullptr);
}

// SIGNAL 31
void StreamingPreferences::swapFaceButtonsChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 31, nullptr);
}

// SIGNAL 32
void StreamingPreferences::captureSysKeysModeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 32, nullptr);
}

// SIGNAL 33
void StreamingPreferences::keepAwakeChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 33, nullptr);
}

// SIGNAL 34
void StreamingPreferences::languageChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 34, nullptr);
}
QT_WARNING_POP
