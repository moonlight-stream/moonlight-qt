#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "popupmanager.h"
#include "identitymanager.h"
#include "nvpairingmanager.h"
#include "nvhttp.h"

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

void MainWindow::closeEvent(QCloseEvent *event)
{
    const QMessageBox::StandardButton ret
        = QMessageBox::warning(this, tr("Application"),
                               tr("something-something-close?"),
                               QMessageBox::Yes | QMessageBox::No);
    switch (ret) {
    case QMessageBox::Yes:
        event->accept();
        break;
    case QMessageBox::No:
    default:
        event->ignore();
        break;
    }
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
