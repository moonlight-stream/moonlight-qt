#include "mainwindow.h"

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
}
