#include "popupmanager.h"

popupmanager::popupmanager();

// this opens a non-blocking informative message telling the user to enter the given pin
// it is open-loop: if the user cancels, nothing happens
// it is expected that upon pairing completion, the ::closePinDialog function will be called.
static void popupmanager::displayPinDialog(QString pin) {

    pinMsgBox = new QMessageBox( this );
    pinMsgBox->setAttribute( Qt::WA_DeleteOnClose ); //makes sure the msgbox is deleted automatically when closed
    pinMsgBox->setStandardButtons( QMessageBox::Ok );
    pinMsgBox->setText("Please enter the number " + pin + " on the GFE dialog on the computer.");
    pinMsgBox->setInformativeText("This dialog will be dismissed once complete.");
    pinMsgBox->open();
}

// to be called when the pairing is complete
static void popupmanager::closePinDialog() {
    pinMsgBox->close();
    delete pinMsgBox;
}

static QString popupmanager::getHostnameDialog() {
    bool ok;
    QString responseHost
        = QInputDialog::getText(this, tr("Add Host Manually"),
                               tr("IP Address or Hostname of GeForce PC"),
                                QLineEdit::Normal,
                                tr("default string"),
                                &ok);
    if (ok && !responseHost.isEmpty()) {
        return responseHost;
    } else {
        return tr("");
    }
}

