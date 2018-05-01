#ifndef POPUPMANAGER_H
#define POPUPMANAGER_H

#include <QtWidgets>

class popupmanager
{
public:
    popupmanager();
    static void displayPinDialog(QString pin, QWidget* parent);
    static void closePinDialog();
    static QString getHostnameDialog(QWidget* parent);

private:
    static QMessageBox *pinMsgBox;
};

#endif // POPUPMANAGER_H
