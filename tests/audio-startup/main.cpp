#define SDL_MAIN_HANDLED
#include "backend/localhostsessions.h"
#include "streaming/audio/renderers/sdl.h"

#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QUuid>
#include <array>
#include <cstring>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

static int pendingAudioMs;
extern "C" int LiGetPendingAudioDuration() { return pendingAudioMs; }

class AudioStartupTest : public QObject
{
    Q_OBJECT
private slots:
    void localAndForwardedHosts()
    {
        const auto id = QUuid::createUuid().toString(QUuid::Id128).toUpper();
        QVERIFY(!LocalHostSessions::isLocalAudioHost({}));
        QVERIFY(!LocalHostSessions::isLocalAudioHost(id));
#ifdef Q_OS_WIN
        const auto name = QByteArrayLiteral("Local\\Sunshine.AudioHost.") + id.toLatin1();
        const auto marker = CreateEventA(nullptr, TRUE, FALSE, name.constData());
        QVERIFY(marker);
        const bool sameDesktop = LocalHostSessions::isLocalAudioHost(id);
        CloseHandle(marker);
        QVERIFY(sameDesktop);
        QVERIFY(!LocalHostSessions::isLocalAudioHost(id));
#endif
    }

    void firstPacketAfterProbeAndReopen()
    {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        const QString path = temp.filePath("startup.pcm");
        qputenv("SDL_AUDIODRIVER", "disk");
        qputenv("SDL_DISKAUDIOFILE", QFile::encodeName(path));
        qputenv("SDL_DISKAUDIOFILEOUT", QFile::encodeName(path));

        OPUS_MULTISTREAM_CONFIGURATION config {};
        config.sampleRate = 48000;
        config.samplesPerFrame = 240;
        config.channelCount = 2;
        {
            SdlAudioRenderer probe;
            QVERIFY(probe.prepareForPlayback(&config));
        }

        std::array<float, 480> pcm;
        for (int i = 0; i < int(pcm.size()); ++i)
            pcm[i] = (i % 7 - 3) / 256.0f;
        const QByteArray expected(reinterpret_cast<const char*>(pcm.data()), sizeof(pcm));
        for (int open = 0; open < 2; ++open) {
            {
                SdlAudioRenderer renderer;
                QVERIFY(renderer.prepareForPlayback(&config));
                QCOMPARE(QByteArray(SDL_GetCurrentAudioDriver()), QByteArray("disk"));
                QVERIFY(renderer.submitAudio(0));
                int size = sizeof(pcm);
                void* buffer = renderer.getAudioBuffer(&size);
                std::memset(buffer, 0x3e, size);
                pendingAudioMs = 35;
                QVERIFY(renderer.submitAudio(size));
                pendingAudioMs = 0;
                std::memcpy(buffer, pcm.data(), sizeof(pcm));
                const auto before = QFileInfo(path).size();
                QVERIFY(renderer.submitAudio(sizeof(pcm)));
                QTRY_VERIFY_WITH_TIMEOUT(QFileInfo(path).size() >= before + 48000, 2000);
            }
            QFile file(path);
            QVERIFY(file.open(QIODevice::ReadOnly));
            const auto output = file.readAll();
            const auto offset = output.indexOf(expected);
            QVERIFY(offset >= 0);
            QCOMPARE(output.left(offset), QByteArray(offset, 0));
        }
    }
};

QTEST_GUILESS_MAIN(AudioStartupTest)
#include "main.moc"
