import QtQuick 2.0
import QtQuick.Dialogs 1.2

import SystemProperties 1.0

MessageDialog {
    property string helpText

    informativeText: SystemProperties.hasBrowser ? helpText : ""
    icon: StandardIcon.Critical
    standardButtons: StandardButton.Ok |
                     (SystemProperties.hasBrowser ? StandardButton.Help : 0)
}
