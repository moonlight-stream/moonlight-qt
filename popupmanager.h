#ifndef POPUPMANAGER_H
#define POPUPMANAGER_H

#include <QWidget>

class popupmanager
{
public:
    popupmanager();
    static void displayPinDialog(QString pin);
    static void closePinDialog();
    static QString getHostnameDialog();

private slots:
    QMessageBox *pinMsgBox = nullptr;
};

#endif // POPUPMANAGER_H
