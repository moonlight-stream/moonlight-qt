#include "popupmanager.h"

QMessageBox *popupmanager::pinMsgBox = nullptr;

popupmanager::popupmanager(){}

// this opens a non-blocking informative message telling the user to enter the given pin
// it is open-loop: if the user cancels, nothing happens
// it is expected that upon pairing completion, the ::closePinDialog function will be called.
void popupmanager::displayPinDialog(QString pin, QWidget* parent) {

    popupmanager::pinMsgBox = new QMessageBox( parent );
    popupmanager::pinMsgBox->setAttribute( Qt::WA_DeleteOnClose ); //makes sure the msgbox is deleted automatically when closed
    popupmanager::pinMsgBox->setStandardButtons( QMessageBox::Ok );
    popupmanager::pinMsgBox->setText("Please enter the number " + pin + " on the GFE dialog on the computer.");
    popupmanager::pinMsgBox->setInformativeText("This dialog will be dismissed once complete.");
    popupmanager::pinMsgBox->open();
}

// to be called when the pairing is complete
void popupmanager::closePinDialog() {
    pinMsgBox->close();
    delete pinMsgBox;
}

QString popupmanager::getHostnameDialog(QWidget* parent) {
    bool ok;
    QString responseHost
        = QInputDialog::getText(parent, QObject::tr("Add Host Manually"),
                               QObject::tr("IP Address or Hostname of GeForce PC"),
                                QLineEdit::Normal,
                                QObject::tr("default string"),
                                &ok);
    if (ok && !responseHost.isEmpty()) {
        return responseHost;
    } else {
        return QObject::tr("");
    }
}


