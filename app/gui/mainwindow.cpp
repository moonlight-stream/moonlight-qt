#include "gui/mainwindow.h"
#include "ui_mainwindow.h"
#include "gui/popupmanager.h"
#include "backend/identitymanager.h"
#include "backend/nvpairingmanager.h"
#include "streaming/streaming.h"
#include "backend/computermanager.h"
#include "backend/boxartmanager.h"

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
    else {
        QImage im = m_BoxArtManager.loadBoxArt(computer, computer->appList[0]);

#if 0
        STREAM_CONFIGURATION sc;
        LiInitializeStreamConfiguration(&sc);
        sc.width = 1280;
        sc.height = 720;
        sc.fps = 60;
        sc.packetSize = 1024;
        sc.bitrate = 10000;
        sc.audioConfiguration = AUDIO_CONFIGURATION_51_SURROUND;

        QMessageBox* box = new QMessageBox(nullptr);
        box->setAttribute(Qt::WA_DeleteOnClose); //makes sure the msgbox is deleted automatically when closed
        box->setStandardButtons(QMessageBox::Cancel);
        box->setText("Launching game...");
        box->open();

        if (computer->currentGameId != 0) {
            http.resumeApp(&sc);
        }
        else {
            http.launchApp(999999, &sc, true, false, SdlGetAttachedGamepadMask());
        }

        SERVER_INFORMATION si;
        QString serverInfo = http.getServerInfo();

        QByteArray hostnameStr = computer->activeAddress.toLatin1();
        QByteArray siAppVersion = http.getXmlString(serverInfo, "appversion").toLatin1();
        QByteArray siGfeVersion = http.getXmlString(serverInfo, "GfeVersion").toLatin1();

        si.address = hostnameStr.data();
        si.serverInfoAppVersion = siAppVersion.data();
        si.serverInfoGfeVersion = siGfeVersion.data();

        StartConnection(&si, &sc, box);
#endif
    }
}

void MainWindow::on_newHostBtn_clicked()
{
    QString hostname = popupmanager::getHostnameDialog(this);
    if (!hostname.isEmpty()) {
        m_ComputerManager.addNewHost(hostname, false);
        QThread::sleep(10);
        m_ComputerManager.stopPollingAsync();
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
