#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
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
