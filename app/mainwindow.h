#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
<<<<<<< HEAD
#include <QLabel>
#include <QPushButton>
=======

namespace Ui {
class MainWindow;
}
>>>>>>> UI-test

class MainWindow : public QMainWindow
{
    Q_OBJECT
<<<<<<< HEAD
public:
    explicit MainWindow(QWidget *parent = nullptr);

private slots:
    void pair();

signals:

private:
    QLabel status_label;
    QPushButton pair_button;
=======

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_pushButton_clicked();

private:
    Ui::MainWindow *ui;
>>>>>>> UI-test
};

#endif // MAINWINDOW_H
