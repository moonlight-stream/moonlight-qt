import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Window 2.2

Rectangle {
    id: banner
    property var monitor
    property bool expanded: false
    readonly property int count: monitor && monitor.available ? monitor.devices.length : 0
    readonly property int controllers: monitor ? monitor.devices.filter(function(device) { return !device.viewOnly }).length : 0
    readonly property bool allClosing: count > 0 && monitor.devices.every(function(device) { return device.closing })
    readonly property bool stacked: expanded && width < 580
    readonly property int inset: expanded ? Math.max(16, Math.min(40, width * 0.044)) : 16
    signal toggled()

    function elapsed(start, now) {
        var seconds = Math.floor(Math.max(0, now - start) / 1000)
        function pad(value) { return value < 10 ? "0" + value : String(value) }
        return pad(Math.floor(seconds / 3600)) + ":" + pad(Math.floor(seconds / 60) % 60) + ":" + pad(seconds % 60)
    }

    implicitHeight: expanded ? Math.max(160, copy.height + (stacked && disconnectAll.visible ? disconnectAll.height : 0) + 48)
                             : Math.max(82, copy.height + 26)
    color: "#337ff4"
    radius: expanded ? 0 : 4
    clip: true

    Image {
        anchors.right: parent.right
        anchors.rightMargin: banner.expanded ? banner.width * 0.14 : 0
        anchors.bottom: parent.bottom
        anchors.bottomMargin: -4
        width: banner.expanded ? Math.min(banner.width * 0.33, 168) : 76
        height: width * 0.875
        source: "qrc:/res/incoming-display.svg"
        sourceSize.width: Math.ceil(width * Screen.devicePixelRatio)
        opacity: 0.14
    }

    Column {
        id: copy
        x: banner.inset
        y: banner.stacked ? 23 : (parent.height - height) / 2
        width: Math.max(1, (banner.expanded && !banner.stacked && disconnectAll.visible ? disconnectAll.x - 24 : banner.width - 26) - x)
        spacing: banner.expanded ? 9 : 4
        Label {
            width: parent.width
            text: !banner.count ? qsTr("Host connection status")
                                : (banner.controllers ? qsTr("This PC is being controlled") : qsTr("This PC is being viewed")) +
                                  (banner.expanded ? "   " + banner.elapsed(monitor.since, monitor.now) : "")
            textFormat: Text.PlainText
            color: "white"
            font.pixelSize: banner.expanded ? (banner.stacked ? 22 : 28) : 16
            font.bold: true
            wrapMode: Text.Wrap
        }
        Label {
            id: summary
            width: parent.width
            text: {
                if (!banner.count)
                    return monitor && monitor.available ? qsTr("No devices connected") : qsTr("Local Sunshine status unavailable")
                if (banner.controllers !== banner.count)
                    return banner.count === 1 ? qsTr("1 device is connected to this PC") : qsTr("%1 devices are connected to this PC").arg(banner.count)
                return banner.count === 1 ? qsTr("1 device is controlling this PC") : qsTr("%1 devices are controlling this PC").arg(banner.count)
            }
            color: "white"
            font.pixelSize: banner.expanded ? 16 : 12
            wrapMode: Text.Wrap
        }
    }

    HostDisconnectButton {
        id: disconnectAll
        visible: banner.expanded && banner.count > 0
        enabled: visible && !monitor.disconnecting && !banner.allClosing
        width: Math.max(122, implicitWidth)
        x: banner.stacked ? banner.inset : banner.width - width - Math.max(banner.inset, banner.width * 0.0625)
        y: banner.stacked ? copy.y + copy.height + 10 : (banner.height - height) / 2 + 16
        text: banner.allClosing ? qsTr("Disconnecting…") : qsTr("Disconnect all")
        onClicked: monitor.disconnectAll()
    }

    ToolButton {
        id: toggle
        anchors { right: parent.right; top: parent.top }
        width: banner.expanded ? 40 : banner.width
        height: banner.expanded ? 40 : banner.height
        hoverEnabled: true
        Accessible.name: banner.expanded ? qsTr("Collapse connection details") : qsTr("Show connection details")
        Accessible.description: summary.text
        contentItem: Text {
            text: banner.expanded ? "\ue73f" : "\ue740"
            font.family: "Segoe MDL2 Assets"
            font.pixelSize: banner.expanded ? 16 : 12
            color: "white"
            horizontalAlignment: Text.AlignRight
            verticalAlignment: Text.AlignTop
            rightPadding: 4
            topPadding: 3
        }
        background: Rectangle {
            radius: 4
            color: toggle.down ? "#26ffffff" : toggle.hovered ? "#12ffffff" : "transparent"
            border.color: "white"
            border.width: toggle.visualFocus ? 2 : 0
        }
        onClicked: banner.toggled()
    }
}
