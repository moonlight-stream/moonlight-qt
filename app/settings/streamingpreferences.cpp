#include "streamingpreferences.h"
#include "utils.h"

#include <QSettings>
#include <QTranslator>
#include <QCoreApplication>
#include <QLocale>
#include <QReadWriteLock>
#include <QtMath>

#include <QtDebug>

#define SER_STREAMSETTINGS "streamsettings"
#define SER_PROFILE_GROUP "profiles"
#define SER_PROFILE_ACTIVE "activeprofile"
#define SER_WIDTH "width"
#define SER_HEIGHT "height"
#define SER_FPS "fps"
#define SER_BITRATE "bitrate"
#define SER_UNLOCK_BITRATE "unlockbitrate"
#define SER_AUTOADJUSTBITRATE "autoadjustbitrate"
#define SER_FULLSCREEN "fullscreen"
#define SER_VSYNC "vsync"
#define SER_GAMEOPTS "gameopts"
#define SER_HOSTAUDIO "hostaudio"
#define SER_MULTICONT "multicontroller"
#define SER_AUDIOCFG "audiocfg"
#define SER_VIDEOCFG "videocfg"
#define SER_HDR "hdr"
#define SER_YUV444 "yuv444"
#define SER_VIDEODEC "videodec"
#define SER_WINDOWMODE "windowmode"
#define SER_MDNS "mdns"
#define SER_QUITAPPAFTER "quitAppAfter"
#define SER_ABSMOUSEMODE "mouseacceleration"
#define SER_ABSTOUCHMODE "abstouchmode"
#define SER_STARTWINDOWED "startwindowed"
#define SER_FRAMEPACING "framepacing"
#define SER_CONNWARNINGS "connwarnings"
#define SER_CONFWARNINGS "confwarnings"
#define SER_UIDISPLAYMODE "uidisplaymode"
#define SER_RICHPRESENCE "richpresence"
#define SER_GAMEPADMOUSE "gamepadmouse"
#define SER_DEFAULTVER "defaultver"
#define SER_PACKETSIZE "packetsize"
#define SER_DETECTNETBLOCKING "detectnetblocking"
#define SER_SHOWPERFOVERLAY "showperfoverlay"
#define SER_SWAPMOUSEBUTTONS "swapmousebuttons"
#define SER_MUTEONFOCUSLOSS "muteonfocusloss"
#define SER_BACKGROUNDGAMEPAD "backgroundgamepad"
#define SER_REVERSESCROLL "reversescroll"
#define SER_SWAPFACEBUTTONS "swapfacebuttons"
#define SER_CAPTURESYSKEYS "capturesyskeys"
#define SER_KEEPAWAKE "keepawake"
#define SER_LANGUAGE "language"

#define CURRENT_DEFAULT_VER 2

static const char kDefaultProfileName[] = "Default";

static StreamingPreferences* s_GlobalPrefs;

Q_GLOBAL_STATIC(QReadWriteLock, s_GlobalPrefsLock)

StreamingPreferences::StreamingPreferences(QQmlEngine *qmlEngine)
    : m_QmlEngine(qmlEngine)
{
    reload();
}

StreamingPreferences* StreamingPreferences::get(QQmlEngine *qmlEngine)
{
    {
        QReadLocker readGuard(s_GlobalPrefsLock);

        // If we have a preference object and it's associated with a QML engine or
        // if the caller didn't specify a QML engine, return the existing object.
        if (s_GlobalPrefs && (s_GlobalPrefs->m_QmlEngine || !qmlEngine)) {
            // The lifetime logic here relies on the QML engine also being a singleton.
            Q_ASSERT(!qmlEngine || s_GlobalPrefs->m_QmlEngine == qmlEngine);
            return s_GlobalPrefs;
        }
    }

    {
        QWriteLocker writeGuard(s_GlobalPrefsLock);

        // If we already have an preference object but the QML engine is now available,
        // associate the QML engine with the preferences.
        if (s_GlobalPrefs) {
            if (!s_GlobalPrefs->m_QmlEngine) {
                s_GlobalPrefs->m_QmlEngine = qmlEngine;
            }
            else {
                // We could reach this codepath if another thread raced with us
                // and created the object while we were outside the pref lock.
                Q_ASSERT(!qmlEngine || s_GlobalPrefs->m_QmlEngine == qmlEngine);
            }
        }
        else {
            s_GlobalPrefs = new StreamingPreferences(qmlEngine);
        }

        return s_GlobalPrefs;
    }
}

QString StreamingPreferences::normalizeProfileName(const QString& name) const
{
    return name.trimmed();
}

bool StreamingPreferences::isProfileNameValid(const QString& name) const
{
    if (name.isEmpty()) {
        return false;
    }

    if (name.contains('/') || name.contains('\\')) {
        return false;
    }

    return true;
}

QStringList StreamingPreferences::loadProfileNames(QSettings& settings)
{
    settings.beginGroup(SER_PROFILE_GROUP);
    QStringList names = settings.childGroups();
    settings.endGroup();

    if (names.isEmpty()) {
        names << kDefaultProfileName;
    }

    names.sort(Qt::CaseInsensitive);

    for (int i = 0; i < names.count(); ++i) {
        if (names[i].compare(kDefaultProfileName, Qt::CaseInsensitive) == 0) {
            if (i != 0) {
                names.move(i, 0);
            }
            break;
        }
    }

    return names;
}

QString StreamingPreferences::resolveProfileName(const QString& name, const QStringList& profiles) const
{
    for (const QString& profileName : profiles) {
        if (profileName.compare(name, Qt::CaseInsensitive) == 0) {
            return profileName;
        }
    }

    return QString();
}

QString StreamingPreferences::profileKey(const QString& key) const
{
    return QString("%1/%2/%3").arg(SER_PROFILE_GROUP, m_CurrentProfile, key);
}

void StreamingPreferences::reload()
{
    QSettings settings;

    QStringList profiles = loadProfileNames(settings);
    QString activeProfile = settings.value(SER_PROFILE_ACTIVE).toString();
    QString resolvedProfile = resolveProfileName(activeProfile, profiles);

    if (resolvedProfile.isEmpty()) {
        resolvedProfile = profiles.first();
        settings.setValue(SER_PROFILE_ACTIVE, resolvedProfile);
    }

    if (m_ProfileNames != profiles) {
        m_ProfileNames = profiles;
        emit profileListChanged();
    }

    if (m_CurrentProfile != resolvedProfile) {
        m_CurrentProfile = resolvedProfile;
        emit currentProfileChanged();
    }

    loadFromSettings(settings, m_CurrentProfile);
    emitAllChangedSignals();
}

void StreamingPreferences::loadFromSettings(QSettings& settings, const QString& profileName)
{
    m_CurrentProfile = profileName;

    auto readValue = [&](const QString& key, const QVariant& defaultValue) {
        return settings.value(profileKey(key), settings.value(key, defaultValue));
    };

    int defaultVer = readValue(SER_DEFAULTVER, 0).toInt();

#ifdef Q_OS_DARWIN
    recommendedFullScreenMode = WindowMode::WM_FULLSCREEN_DESKTOP;
#else
    // Wayland doesn't support modesetting, so use fullscreen desktop mode
    // unless we have a slow GPU (which can take advantage of wp_viewporter
    // to reduce GPU load with lower resolution video streams).
    if (WMUtils::isRunningWayland() && !WMUtils::isGpuSlow()) {
        recommendedFullScreenMode = WindowMode::WM_FULLSCREEN_DESKTOP;
    }
    else {
        recommendedFullScreenMode = WindowMode::WM_FULLSCREEN;
    }
#endif

    width = readValue(SER_WIDTH, 1280).toInt();
    height = readValue(SER_HEIGHT, 720).toInt();
    fps = readValue(SER_FPS, 60).toInt();
    enableYUV444 = readValue(SER_YUV444, false).toBool();
    bitrateKbps = readValue(SER_BITRATE, getDefaultBitrate(width, height, fps, enableYUV444)).toInt();
    unlockBitrate = readValue(SER_UNLOCK_BITRATE, false).toBool();
    autoAdjustBitrate = readValue(SER_AUTOADJUSTBITRATE, true).toBool();
    enableVsync = readValue(SER_VSYNC, true).toBool();
    gameOptimizations = readValue(SER_GAMEOPTS, true).toBool();
    playAudioOnHost = readValue(SER_HOSTAUDIO, false).toBool();
    multiController = readValue(SER_MULTICONT, true).toBool();
    enableMdns = readValue(SER_MDNS, true).toBool();
    quitAppAfter = readValue(SER_QUITAPPAFTER, false).toBool();
    absoluteMouseMode = readValue(SER_ABSMOUSEMODE, false).toBool();
    absoluteTouchMode = readValue(SER_ABSTOUCHMODE, true).toBool();
    framePacing = readValue(SER_FRAMEPACING, false).toBool();
    connectionWarnings = readValue(SER_CONNWARNINGS, true).toBool();
    configurationWarnings = readValue(SER_CONFWARNINGS, true).toBool();
    richPresence = readValue(SER_RICHPRESENCE, true).toBool();
    gamepadMouse = readValue(SER_GAMEPADMOUSE, true).toBool();
    detectNetworkBlocking = readValue(SER_DETECTNETBLOCKING, true).toBool();
    showPerformanceOverlay = readValue(SER_SHOWPERFOVERLAY, false).toBool();
    packetSize = readValue(SER_PACKETSIZE, 0).toInt();
    swapMouseButtons = readValue(SER_SWAPMOUSEBUTTONS, false).toBool();
    muteOnFocusLoss = readValue(SER_MUTEONFOCUSLOSS, false).toBool();
    backgroundGamepad = readValue(SER_BACKGROUNDGAMEPAD, false).toBool();
    reverseScrollDirection = readValue(SER_REVERSESCROLL, false).toBool();
    swapFaceButtons = readValue(SER_SWAPFACEBUTTONS, false).toBool();
    keepAwake = readValue(SER_KEEPAWAKE, true).toBool();
    enableHdr = readValue(SER_HDR, false).toBool();
    captureSysKeysMode = static_cast<CaptureSysKeysMode>(readValue(SER_CAPTURESYSKEYS,
                                                         static_cast<int>(CaptureSysKeysMode::CSK_OFF)).toInt());
    audioConfig = static_cast<AudioConfig>(readValue(SER_AUDIOCFG,
                                                  static_cast<int>(AudioConfig::AC_STEREO)).toInt());
    videoCodecConfig = static_cast<VideoCodecConfig>(readValue(SER_VIDEOCFG,
                                                  static_cast<int>(VideoCodecConfig::VCC_AUTO)).toInt());
    videoDecoderSelection = static_cast<VideoDecoderSelection>(readValue(SER_VIDEODEC,
                                                  static_cast<int>(VideoDecoderSelection::VDS_AUTO)).toInt());
    windowMode = static_cast<WindowMode>(readValue(SER_WINDOWMODE,
                                                        // Try to load from the old preference value too
                                                        static_cast<int>(settings.value(SER_FULLSCREEN, true).toBool() ?
                                                                             recommendedFullScreenMode : WindowMode::WM_WINDOWED)).toInt());
    uiDisplayMode = static_cast<UIDisplayMode>(readValue(SER_UIDISPLAYMODE,
                                               static_cast<int>(settings.value(SER_STARTWINDOWED, true).toBool() ? UIDisplayMode::UI_WINDOWED
                                                                                                                 : UIDisplayMode::UI_MAXIMIZED)).toInt());
    language = static_cast<Language>(readValue(SER_LANGUAGE,
                                                    static_cast<int>(Language::LANG_AUTO)).toInt());


    // Perform default settings updates as required based on last default version
    if (defaultVer < 1) {
#ifdef Q_OS_DARWIN
        // Update window mode setting on macOS from full-screen (old default) to borderless windowed (new default)
        if (windowMode == WindowMode::WM_FULLSCREEN) {
            windowMode = WindowMode::WM_FULLSCREEN_DESKTOP;
        }
#endif
    }
    if (defaultVer < 2) {
        if (windowMode == WindowMode::WM_FULLSCREEN && WMUtils::isRunningWayland()) {
            windowMode = WindowMode::WM_FULLSCREEN_DESKTOP;
        }
    }

    // Fixup VCC value to the new settings format with codec and HDR separate
    if (videoCodecConfig == VCC_FORCE_HEVC_HDR_DEPRECATED) {
        videoCodecConfig = VCC_AUTO;
        enableHdr = true;
    }
}

QString StreamingPreferences::currentProfile() const
{
    return m_CurrentProfile;
}

QStringList StreamingPreferences::profileNames() const
{
    return m_ProfileNames;
}

void StreamingPreferences::setCurrentProfile(const QString& name)
{
    QString normalizedName = normalizeProfileName(name);
    if (!isProfileNameValid(normalizedName)) {
        return;
    }

    QSettings settings;
    QStringList profiles = loadProfileNames(settings);
    QString resolvedProfile = resolveProfileName(normalizedName, profiles);

    if (resolvedProfile.isEmpty() || resolvedProfile == m_CurrentProfile) {
        return;
    }

    save();

    m_CurrentProfile = resolvedProfile;
    settings.setValue(SER_PROFILE_ACTIVE, m_CurrentProfile);

    loadFromSettings(settings, m_CurrentProfile);
    emitAllChangedSignals();

    if (m_ProfileNames != profiles) {
        m_ProfileNames = profiles;
        emit profileListChanged();
    }

    emit currentProfileChanged();
}

bool StreamingPreferences::createProfile(const QString& name)
{
    QString normalizedName = normalizeProfileName(name);
    if (!isProfileNameValid(normalizedName)) {
        return false;
    }

    QSettings settings;
    QStringList profiles = loadProfileNames(settings);
    if (!resolveProfileName(normalizedName, profiles).isEmpty()) {
        return false;
    }

    save();
    saveToSettings(settings, normalizedName);
    settings.setValue(SER_PROFILE_ACTIVE, normalizedName);

    setCurrentProfile(normalizedName);
    return true;
}

bool StreamingPreferences::deleteProfile(const QString& name)
{
    QString normalizedName = normalizeProfileName(name);
    if (!isProfileNameValid(normalizedName)) {
        return false;
    }

    QSettings settings;
    QStringList profiles = loadProfileNames(settings);
    QString resolvedProfile = resolveProfileName(normalizedName, profiles);

    if (resolvedProfile.isEmpty() || profiles.count() <= 1) {
        return false;
    }

    settings.remove(QString("%1/%2").arg(SER_PROFILE_GROUP, resolvedProfile));

    profiles = loadProfileNames(settings);

    if (m_ProfileNames != profiles) {
        m_ProfileNames = profiles;
        emit profileListChanged();
    }

    if (m_CurrentProfile == resolvedProfile) {
        m_CurrentProfile = profiles.first();
        settings.setValue(SER_PROFILE_ACTIVE, m_CurrentProfile);
        loadFromSettings(settings, m_CurrentProfile);
        emitAllChangedSignals();
        emit currentProfileChanged();
    }

    return true;
}

bool StreamingPreferences::retranslate()
{
    static QTranslator* translator = nullptr;

#if QT_VERSION < QT_VERSION_CHECK(5, 10, 0)
    if (m_QmlEngine != nullptr) {
        // Dynamic retranslation is not supported until Qt 5.10
        return false;
    }
#endif

    QTranslator* newTranslator = new QTranslator();
    QString languageSuffix = getSuffixFromLanguage(language);

    // Remove the old translator, even if we can't load a new one.
    // Otherwise we'll be stuck with the old translated values instead
    // of defaulting to English.
    if (translator != nullptr) {
        QCoreApplication::removeTranslator(translator);
        delete translator;
        translator = nullptr;
    }

    if (newTranslator->load(QString(":/languages/qml_") + languageSuffix)) {
        qInfo() << "Successfully loaded translation for" << languageSuffix;

        translator = newTranslator;
        QCoreApplication::installTranslator(translator);
    }
    else {
        qInfo() << "No translation available for" << languageSuffix;
        delete newTranslator;
    }

    if (m_QmlEngine != nullptr) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 10, 0)
        // This is a dynamic retranslation from the settings page.
        // We have to kick the QML engine into reloading our text.
        m_QmlEngine->retranslate();
#else
        // Unreachable below Qt 5.10 due to the check above
        Q_ASSERT(false);
#endif
    }
    else {
        // This is a translation from a non-QML context, which means
        // it is probably app startup. There's nothing to refresh.
    }

    return true;
}

QString StreamingPreferences::getSuffixFromLanguage(StreamingPreferences::Language lang)
{
    switch (lang)
    {
    case LANG_DE:
        return "de";
    case LANG_EN:
        return "en";
    case LANG_FR:
        return "fr";
    case LANG_ZH_CN:
        return "zh_CN";
    case LANG_NB_NO:
        return "nb_NO";
    case LANG_RU:
        return "ru";
    case LANG_ES:
        return "es";
    case LANG_JA:
        return "ja";
    case LANG_VI:
        return "vi";
    case LANG_TH:
        return "th";
    case LANG_KO:
        return "ko";
    case LANG_HU:
        return "hu";
    case LANG_NL:
        return "nl";
    case LANG_SV:
        return "sv";
    case LANG_TR:
        return "tr";
    case LANG_UK:
        return "uk";
    case LANG_ZH_TW:
        return "zh_TW";
    case LANG_PT:
        return "pt";
    case LANG_PT_BR:
        return "pt_BR";
    case LANG_EL:
        return "el";
    case LANG_IT:
        return "it";
    case LANG_HI:
        return "hi";
    case LANG_PL:
        return "pl";
    case LANG_CS:
        return "cs";
    case LANG_HE:
        return "he";
    case LANG_CKB:
        return "ckb";
    case LANG_LT:
        return "lt";
    case LANG_ET:
        return "et";
    case LANG_BG:
        return "bg";
    case LANG_EO:
        return "eo";
    case LANG_TA:
        return "ta";
    case LANG_AUTO:
    default:
        return QLocale::system().name();
    }
}

void StreamingPreferences::saveToSettings(QSettings& settings, const QString& profileName)
{
    auto key = [&](const QString& keyName) {
        return QString("%1/%2/%3").arg(SER_PROFILE_GROUP, profileName, keyName);
    };

    settings.setValue(key(SER_WIDTH), width);
    settings.setValue(key(SER_HEIGHT), height);
    settings.setValue(key(SER_FPS), fps);
    settings.setValue(key(SER_BITRATE), bitrateKbps);
    settings.setValue(key(SER_UNLOCK_BITRATE), unlockBitrate);
    settings.setValue(key(SER_AUTOADJUSTBITRATE), autoAdjustBitrate);
    settings.setValue(key(SER_VSYNC), enableVsync);
    settings.setValue(key(SER_GAMEOPTS), gameOptimizations);
    settings.setValue(key(SER_HOSTAUDIO), playAudioOnHost);
    settings.setValue(key(SER_MULTICONT), multiController);
    settings.setValue(key(SER_MDNS), enableMdns);
    settings.setValue(key(SER_QUITAPPAFTER), quitAppAfter);
    settings.setValue(key(SER_ABSMOUSEMODE), absoluteMouseMode);
    settings.setValue(key(SER_ABSTOUCHMODE), absoluteTouchMode);
    settings.setValue(key(SER_FRAMEPACING), framePacing);
    settings.setValue(key(SER_CONNWARNINGS), connectionWarnings);
    settings.setValue(key(SER_CONFWARNINGS), configurationWarnings);
    settings.setValue(key(SER_RICHPRESENCE), richPresence);
    settings.setValue(key(SER_GAMEPADMOUSE), gamepadMouse);
    settings.setValue(key(SER_PACKETSIZE), packetSize);
    settings.setValue(key(SER_DETECTNETBLOCKING), detectNetworkBlocking);
    settings.setValue(key(SER_SHOWPERFOVERLAY), showPerformanceOverlay);
    settings.setValue(key(SER_AUDIOCFG), static_cast<int>(audioConfig));
    settings.setValue(key(SER_HDR), enableHdr);
    settings.setValue(key(SER_YUV444), enableYUV444);
    settings.setValue(key(SER_VIDEOCFG), static_cast<int>(videoCodecConfig));
    settings.setValue(key(SER_VIDEODEC), static_cast<int>(videoDecoderSelection));
    settings.setValue(key(SER_WINDOWMODE), static_cast<int>(windowMode));
    settings.setValue(key(SER_UIDISPLAYMODE), static_cast<int>(uiDisplayMode));
    settings.setValue(key(SER_LANGUAGE), static_cast<int>(language));
    settings.setValue(key(SER_DEFAULTVER), CURRENT_DEFAULT_VER);
    settings.setValue(key(SER_SWAPMOUSEBUTTONS), swapMouseButtons);
    settings.setValue(key(SER_MUTEONFOCUSLOSS), muteOnFocusLoss);
    settings.setValue(key(SER_BACKGROUNDGAMEPAD), backgroundGamepad);
    settings.setValue(key(SER_REVERSESCROLL), reverseScrollDirection);
    settings.setValue(key(SER_SWAPFACEBUTTONS), swapFaceButtons);
    settings.setValue(key(SER_CAPTURESYSKEYS), captureSysKeysMode);
    settings.setValue(key(SER_KEEPAWAKE), keepAwake);
}

void StreamingPreferences::save()
{
    QSettings settings;

    if (m_CurrentProfile.isEmpty()) {
        m_CurrentProfile = kDefaultProfileName;
    }

    settings.setValue(SER_PROFILE_ACTIVE, m_CurrentProfile);
    saveToSettings(settings, m_CurrentProfile);
}

void StreamingPreferences::emitAllChangedSignals()
{
    emit displayModeChanged();
    emit bitrateChanged();
    emit unlockBitrateChanged();
    emit autoAdjustBitrateChanged();
    emit enableVsyncChanged();
    emit gameOptimizationsChanged();
    emit playAudioOnHostChanged();
    emit multiControllerChanged();
    emit unsupportedFpsChanged();
    emit enableMdnsChanged();
    emit quitAppAfterChanged();
    emit absoluteMouseModeChanged();
    emit absoluteTouchModeChanged();
    emit audioConfigChanged();
    emit videoCodecConfigChanged();
    emit enableHdrChanged();
    emit enableYUV444Changed();
    emit videoDecoderSelectionChanged();
    emit uiDisplayModeChanged();
    emit windowModeChanged();
    emit framePacingChanged();
    emit connectionWarningsChanged();
    emit configurationWarningsChanged();
    emit richPresenceChanged();
    emit gamepadMouseChanged();
    emit detectNetworkBlockingChanged();
    emit showPerformanceOverlayChanged();
    emit mouseButtonsChanged();
    emit muteOnFocusLossChanged();
    emit backgroundGamepadChanged();
    emit reverseScrollDirectionChanged();
    emit swapFaceButtonsChanged();
    emit captureSysKeysModeChanged();
    emit keepAwakeChanged();
    emit languageChanged();
}

int StreamingPreferences::getDefaultBitrate(int width, int height, int fps, bool yuv444)
{
    // Don't scale bitrate linearly beyond 60 FPS. It's definitely not a linear
    // bitrate increase for frame rate once we get to values that high.
    float frameRateFactor = (fps <= 60 ? fps : (qSqrt(fps / 60.f) * 60.f)) / 30.f;

    // TODO: Collect some empirical data to see if these defaults make sense.
    // We're just using the values that the Shield used, as we have for years.
    static const struct resTable {
        int pixels;
        int factor;
    } resTable[] {
        { 640 * 360, 1 },
        { 854 * 480, 2 },
        { 1280 * 720, 5 },
        { 1920 * 1080, 10 },
        { 2560 * 1440, 20 },
        { 3840 * 2160, 40 },
        { -1, -1 },
    };

    // Calculate the resolution factor by linear interpolation of the resolution table
    float resolutionFactor;
    int pixels = width * height;
    for (int i = 0;; i++) {
        if (pixels == resTable[i].pixels) {
            // We can bail immediately for exact matches
            resolutionFactor = resTable[i].factor;
            break;
        }
        else if (pixels < resTable[i].pixels) {
            if (i == 0) {
                // Never go below the lowest resolution entry
                resolutionFactor = resTable[i].factor;
            }
            else {
                // Interpolate between the entry greater than the chosen resolution (i) and the entry less than the chosen resolution (i-1)
                resolutionFactor = ((float)(pixels - resTable[i-1].pixels) / (resTable[i].pixels - resTable[i-1].pixels)) * (resTable[i].factor - resTable[i-1].factor) + resTable[i-1].factor;
            }
            break;
        }
        else if (resTable[i].pixels == -1) {
            // Never go above the highest resolution entry
            resolutionFactor = resTable[i-1].factor;
            break;
        }
    }

    if (yuv444) {
        // This is rough estimation based on the fact that 4:4:4 doubles the amount of raw YUV data compared to 4:2:0
        resolutionFactor *= 2;
    }

    return qRound(resolutionFactor * frameRateFactor) * 1000;
}
