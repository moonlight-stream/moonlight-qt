import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Controls.Material 2.2
import QtQuick.Layouts 1.3

FocusScope {
    id: page
    property var monitor
    objectName: qsTr("Incoming connections")
    readonly property int inset: Math.max(12, Math.min(40, width * 0.044))
    Material.theme: Material.Light

    Connections {
        target: page.monitor
        function onStatusChanged() {
            if (page.monitor.available && !page.monitor.devices.length && stackView.currentItem === page)
                stackView.pop()
        }
    }
    Rectangle { anchors.fill: parent; color: "#f8fafc" }
    HostSessionBanner {
        id: banner
        anchors { left: parent.left; right: parent.right; top: parent.top }
        height: implicitHeight
        monitor: page.monitor
        expanded: true
        onToggled: stackView.pop()
    }
    ListView {
        id: list
        anchors { top: banner.bottom; bottom: parent.bottom; left: parent.left; right: parent.right }
        anchors.leftMargin: page.inset
        anchors.rightMargin: page.inset
        flickableDirection: Flickable.VerticalFlick
        topMargin: Math.min(26, page.inset)
        bottomMargin: 12
        spacing: 6
        clip: true
        model: page.monitor.available ? page.monitor.devices : []
        ScrollBar.vertical: AppScrollBar {}
        header: Label {
            visible: list.count > 0
            height: visible ? implicitHeight + 8 : 0
            text: qsTr("Connected devices")
            color: "#27384c"
            font.pixelSize: 15
        }
        delegate: Rectangle {
            id: row
            width: list.width
            height: contents.implicitHeight + 32
            color: "white"
            border.color: "#dce2e9"
            radius: 6
            readonly property bool narrow: width < 380
            RowLayout {
                id: contents
                anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: row.narrow ? 10 : 16 }
                spacing: row.narrow ? 8 : 14
                Rectangle {
                    visible: !row.narrow
                    Layout.preferredWidth: 40; Layout.preferredHeight: 40
                    radius: 6; color: "#3382ff"
                    Image {
                        anchors.centerIn: parent
                        width: 23; height: 23
                        source: "qrc:/res/desktop_windows-48px.svg"
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 3
                    Label {
                        Layout.fillWidth: true
                        text: modelData.name
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        font.pixelSize: 16; font.weight: Font.Medium
                        color: "#182b40"
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 5
                        Rectangle {
                            width: mode.implicitWidth + 8
                            height: mode.implicitHeight + 2
                            radius: 3; color: "white"; border.color: "#ced7e1"
                            Label {
                                id: mode
                                anchors.centerIn: parent
                                text: modelData.viewOnly ? qsTr("View only") : qsTr("Remote control")
                                font.pixelSize: 12; color: "#52647a"
                            }
                        }
                        Label {
                            width: Math.min(implicitWidth, parent.width)
                            text: qsTr("Connected for %1").arg(banner.elapsed(modelData.since, page.monitor.now))
                            topPadding: 1
                            wrapMode: Text.Wrap
                            font.pixelSize: 12; color: "#52647a"
                        }
                    }
                }
                HostDisconnectButton {
                    text: modelData.closing ? qsTranslate("HostSessionBanner", "Disconnecting…") : qsTr("Disconnect")
                    Accessible.name: qsTr("Disconnect %1").arg(modelData.name)
                    enabled: !page.monitor.disconnecting && !modelData.closing
                    onClicked: page.monitor.disconnectDevice(modelData.id)
                }
            }
        }
    }
}
