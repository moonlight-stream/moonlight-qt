#include "mainwindow.h"
<<<<<<< HEAD

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent),
    status_label(this),
    pair_button(this)
{
    status_label.setFrameRect(QRect(10,10,100,100));
    status_label.setText("not paired");

    pair_button.move(10,30);
    pair_button.setFixedSize(100,20);
    pair_button.setText("Pair");

    connect(&pair_button, SIGNAL(clicked()), this, SLOT(pair()));
}

void MainWindow::pair()
{
    status_label.setText("paired");
=======
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
>>>>>>> UI-test
}
