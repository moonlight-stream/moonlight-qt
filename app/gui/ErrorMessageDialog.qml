import QtQuick 2.0
import QtQuick.Dialogs 1.2

import SystemProperties 1.0

NavigableMessageDialog {
    property string helpText
    property string helpUrl : "https://github.com/moonlight-stream/moonlight-docs/wiki/Troubleshooting"

    informativeText: SystemProperties.hasBrowser ? helpText : ""
    icon: StandardIcon.Critical
    standardButtons: StandardButton.Ok |
                     (SystemProperties.hasBrowser ? StandardButton.Help : 0)

    onHelp: {
        Qt.openUrlExternally(helpUrl)
    }
}
