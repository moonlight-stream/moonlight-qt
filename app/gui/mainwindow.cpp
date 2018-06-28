#include "gui/mainwindow.h"
#include "ui_mainwindow.h"
#include "gui/popupmanager.h"
#include "backend/identitymanager.h"
#include "backend/nvpairingmanager.h"
#include "streaming/streaming.h"
#include "backend/computermanager.h"
#include "backend/boxartmanager.h"
#include "settings/streamingpreferences.h"

#include <QRandomGenerator>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    m_BoxArtManager(this),
    m_ComputerManager(this)
{
    ui->setupUi(this);
    connect(&m_BoxArtManager, SIGNAL(boxArtLoadComplete(NvComputer*,NvApp,QImage)),
            this, SLOT(boxArtLoadComplete(NvComputer*,NvApp,QImage)));
    connect(&m_ComputerManager, SIGNAL(computerStateChanged(NvComputer*)),
            this, SLOT(computerStateChanged(NvComputer*)));
    m_ComputerManager.startPolling();
    qDebug() << "Cached computers: " << m_ComputerManager.getComputers().count();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::boxArtLoadComplete(NvComputer* computer, NvApp app, QImage image)
{
    qDebug() << "Loaded image";
}

void MainWindow::on_actionExit_triggered()
{
    exit(EXIT_SUCCESS);
}

void MainWindow::computerStateChanged(NvComputer* computer)
{
    QReadLocker lock(&computer->lock);

    NvHTTP http(computer->activeAddress);

    if (computer->pairState == NvComputer::PS_NOT_PAIRED) {
        NvPairingManager pm(computer->activeAddress);
        QString pin = pm.generatePinString();
        pm.pair(http.getServerInfo(), pin);
    }
    else if (!computer->appList.isEmpty()) {
        QImage im = m_BoxArtManager.loadBoxArt(computer, computer->appList[0]);

        // Stop polling before launching a game
        m_ComputerManager.stopPollingAsync();

#if 1
        STREAM_CONFIGURATION streamConfig;
        StreamingPreferences prefs;

        LiInitializeStreamConfiguration(&streamConfig);
        // TODO: Validate 4K and 4K60 based on serverinfo
        streamConfig.width = prefs.width;
        streamConfig.height = prefs.height;
        streamConfig.fps = prefs.fps;
        streamConfig.bitrate = prefs.bitrateKbps;
        streamConfig.packetSize = 1024;
        streamConfig.hevcBitratePercentageMultiplier = 75;
        for (int i = 0; i < sizeof(streamConfig.remoteInputAesKey); i++) {
            streamConfig.remoteInputAesKey[i] =
                    (char)(QRandomGenerator::global()->generate() % 256);
        }
        *(int*)streamConfig.remoteInputAesIv = qToBigEndian(QRandomGenerator::global()->generate());
        switch (prefs.audioConfig)
        {
        case StreamingPreferences::AC_AUTO:
            streamConfig.audioConfiguration = SdlDetermineAudioConfiguration();
            break;
        case StreamingPreferences::AC_FORCE_STEREO:
            streamConfig.audioConfiguration = AUDIO_CONFIGURATION_STEREO;
            break;
        case StreamingPreferences::AC_FORCE_SURROUND:
            streamConfig.audioConfiguration = AUDIO_CONFIGURATION_51_SURROUND;
            break;
        }
        switch (prefs.videoCodecConfig)
        {
        case StreamingPreferences::VCC_AUTO:
            // TODO: Determine if HEVC is better depending on the decoder
            streamConfig.supportsHevc = true;
            streamConfig.enableHdr = false;
            break;
        case StreamingPreferences::VCC_FORCE_H264:
            streamConfig.supportsHevc = false;
            streamConfig.enableHdr = false;
            break;
        case StreamingPreferences::VCC_FORCE_HEVC:
            streamConfig.supportsHevc = true;
            streamConfig.enableHdr = false;
            break;
        case StreamingPreferences::VCC_FORCE_HEVC_HDR:
            streamConfig.supportsHevc = true;
            streamConfig.enableHdr = true;
            break;
        }

        // TODO: Validate HEVC support based on decoder caps
        // TODO: Validate HDR support based on decoder caps, display, server caps, and app

        // Initialize the gamepad code with our preferences
        // Note: must be done before SdlGetAttachedGamepadMask!
        SdlInitializeGamepad(prefs.multiController);

        QMessageBox* box = new QMessageBox(nullptr);
        box->setAttribute(Qt::WA_DeleteOnClose); //makes sure the msgbox is deleted automatically when closed
        box->setStandardButtons(QMessageBox::Cancel);
        box->setText("Launching game...");
        box->open();

        if (computer->currentGameId != 0) {
            http.resumeApp(&streamConfig);
        }
        else {
            http.launchApp(999999, &streamConfig,
                           prefs.enableGameOptimizations,
                           prefs.playAudioOnHost,
                           SdlGetAttachedGamepadMask());
        }

        SERVER_INFORMATION hostInfo;
        QString serverInfo = http.getServerInfo();

        QByteArray hostnameStr = computer->activeAddress.toLatin1();
        QByteArray siAppVersion = http.getXmlString(serverInfo, "appversion").toLatin1();
        QByteArray siGfeVersion = http.getXmlString(serverInfo, "GfeVersion").toLatin1();

        hostInfo.address = hostnameStr.data();
        hostInfo.serverInfoAppVersion = siAppVersion.data();
        hostInfo.serverInfoGfeVersion = siGfeVersion.data();

        StartConnection(&hostInfo, &streamConfig, box);
#endif
    }
}

void MainWindow::on_newHostBtn_clicked()
{
    QString hostname = popupmanager::getHostnameDialog(this);
    if (!hostname.isEmpty()) {
        m_ComputerManager.addNewHost(hostname, false);
    }
}

void MainWindow::addHostToDisplay(QMap<QString, bool> hostMdnsMap) {

    QMapIterator<QString, bool> i(hostMdnsMap);
    while (i.hasNext()) {
        i.next();
        ui->hostSelectCombo->addItem(i.key());
        // we can ignore the mdns for now, it's only useful for displaying unpairing options
    }
}

void MainWindow::on_selectHostComboBox_activated(const QString &selectedHostname)
{
    // TODO: get all the applications that "selectedHostname" has listed
    // probably populate another combobox of applications for the time being
}
