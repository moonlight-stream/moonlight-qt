#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    myButton = new QPushButton(this);
    myButton->setIcon(QIcon(":/res/icon128.png"));
    myButton->setIconSize(QSize(128, 128));
    myButton->resize(QSize(128, 128));
    connect(myButton, &QAbstractButton::clicked, this, &MainWindow::on_actionExit_triggered);
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
    delete myButton;
    delete pinMsgBox;
}

void MainWindow::on_actionExit_triggered()
{
    exit(EXIT_SUCCESS);
}

void MainWindow::on_newHostBtn_clicked()
{
    bool ok;
    QString responseHost
        = QInputDialog::getText(this, tr("Add Host Manually"),
                               tr("IP Address or Hostname of GeForce PC"),
                                QLineEdit::Normal,
                                tr("default string"),
                                &ok);
    if (ok && !responseHost.isEmpty()) {
        // TODO: send pair request to "responseHost"
    } else {
        // silently close, user canceled
    }
}

// this opens a non-blocking informative message telling the user to enter the given pin
// it is open-loop: if the user cancels, nothing happens
// it is expected that upon pairing completion, the ::closePinDialog function will be called.
void MainWindow::displayPinDialog(QString pin = tr("ERROR")) {

    pinMsgBox = new QMessageBox( this );
    pinMsgBox->setAttribute( Qt::WA_DeleteOnClose ); //makes sure the msgbox is deleted automatically when closed
    pinMsgBox->setStandardButtons( QMessageBox::Ok );
    pinMsgBox->setText("Please enter the number " + pin + " on the GFE dialog on the computer.");
    pinMsgBox->setInformativeText("This dialog will be dismissed once complete.");
    pinMsgBox->open();
}

// to be called when the pairing is complete
void MainWindow::closePinDialog() {
    pinMsgBox->close();
}
