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
}

void MainWindow::on_actionExit_triggered()
{
    exit(EXIT_SUCCESS);
}
