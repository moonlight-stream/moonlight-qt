#include "gui/mainwindow.h"
#include "ui_mainwindow.h"
#include "gui/popupmanager.h"
#include "http/identitymanager.h"
#include "http/nvpairingmanager.h"
#include "http/nvhttp.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

// sample code for an iconized button performing an action
// will be useful to implement the game grid UI later
//    myButton = new QPushButton(this);
//    myButton->setIcon(QIcon(":/res/icon128.png"));
//    myButton->setIconSize(QSize(128, 128));
//    myButton->resize(QSize(128, 128));
//    connect(myButton, &QAbstractButton::clicked, this, &MainWindow::on_actionExit_triggered);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_actionExit_triggered()
{
    exit(EXIT_SUCCESS);
}

void MainWindow::on_newHostBtn_clicked()
{
    QString hostname = popupmanager::getHostnameDialog(this);
    if (!hostname.isEmpty()) {

        IdentityManager im = IdentityManager();
        NvPairingManager pm(hostname, im);

        QString pin = pm.generatePinString();
        NvHTTP http(hostname, im);
        pm.pair(http.getServerInfo(), pin);
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
