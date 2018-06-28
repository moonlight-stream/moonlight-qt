#include "gui/mainwindow.h"
#include "ui_mainwindow.h"
#include "gui/popupmanager.h"
#include "backend/identitymanager.h"
#include "backend/nvpairingmanager.h"
#include "streaming/session.hpp"
#include "backend/computermanager.h"
#include "backend/boxartmanager.h"
#include "settings/streamingpreferences.h"

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

        Session session(computer, computer->appList.last());
        QStringList warnings;
        QString errorMessage;

        // First check for a fatal configuration error
        errorMessage = session.checkForFatalValidationError();
        if (!errorMessage.isEmpty()) {
            // TODO: display error dialog
            goto AfterStreaming;
        }

        // Check for any informational messages to display
        warnings = session.checkForAdvisoryValidationError();
        if (!warnings.isEmpty()) {
            // TODO: display toast or something before we start
        }

        // Run the streaming session until termination
        session.exec();

    AfterStreaming:
        m_ComputerManager.startPolling();
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
