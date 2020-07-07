#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include <backend/computermanager.h>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_connect_pushButton_clicked();

    void on_pair_pushButton_clicked();

    void computerConnected(QVariant);

    void computerPaired(NvComputer*, QString);

private:
    Ui::MainWindow *ui;

    ComputerManager computerManager;
};

#endif // MAINWINDOW_H
