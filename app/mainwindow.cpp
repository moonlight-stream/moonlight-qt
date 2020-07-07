#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(&computerManager, SIGNAL(computerAddCompleted(QVariant)), this, SLOT(computerConnected(QVariant)));
    connect(&computerManager, SIGNAL(pairingCompleted(NvComputer*, QString)), this, SLOT(computerPaired(NvComputer*, QString)));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_connect_pushButton_clicked()
{
    ui->status_label->setText("connecting... (127.0.0.1)");
    computerManager.addNewHost("127.0.0.1", false);
}

void MainWindow::on_pair_pushButton_clicked()
{
    ui->status_label->setText("pairing... (1111)");
    QVector<NvComputer*> computers = computerManager.getComputers();
    if(computers.isEmpty() == true || computers[0]->state != NvComputer::CS_ONLINE)
    {
        ui->status_label->setText("please connect first!");
    }
    else
    {
        ui->status_label->setText("pairing... (1111)");
        computerManager.pairHost(computers[0], "1111");
    }
}

void MainWindow::computerConnected(QVariant success)
{
    if (success.toBool() == true)
    {
        ui->status_label->setText("computer successfully connected");
    }
    else
    {
        ui->status_label->setText("computer falied to connect");
    }
}

void MainWindow::computerPaired(NvComputer* computer, QString)
{
    if (computer->pairState == NvComputer::PS_PAIRED)
    {
        ui->status_label->setText("computer successfully paired");
    }
    else
    {
        ui->status_label->setText("computer paired failed");
    }
}
