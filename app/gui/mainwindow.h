#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "backend/computermanager.h"
#include "backend/boxartmanager.h"

#include <QMainWindow>
#include <QtWidgets>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_actionExit_triggered();
    void on_newHostBtn_clicked();
    void addHostToDisplay(QMap<QString, bool>);
    void on_selectHostComboBox_activated(const QString &);
    void computerStateChanged(NvComputer* computer);
    void boxArtLoadComplete(NvComputer* computer, NvApp app, QImage image);

private:
    Ui::MainWindow *ui;
    BoxArtManager m_BoxArtManager;
    ComputerManager m_ComputerManager;
};

#endif // MAINWINDOW_H
