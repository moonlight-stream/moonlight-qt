#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    qInfo("test1");
}

MainWindow::~MainWindow()
{
    delete ui;
    qInfo("test3");
}

void MainWindow::on_pushButton_clicked()
{
    close();
    qInfo("test2");
}
