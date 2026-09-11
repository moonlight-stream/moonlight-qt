import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.3

import ComputerManager 1.0
import GilCoordinator 1.0

Item {
    id: loginView
    objectName: qsTr("GilStreaming")
    focus: true

    Connections {
        target: GilCoordinator
        function onAssignedHost(address, port) {
            ComputerManager.addAssignedHost(address, port)
            stackView.replace("qrc:/gui/PcView.qml")
        }
    }

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - 80, 520)
        spacing: 20

        Label {
            Layout.fillWidth: true
            text: qsTr("GilStreaming")
            font.pointSize: 34
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
        }

        Label {
            Layout.fillWidth: true
            text: GilCoordinator.statusText
            font.pointSize: 16
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.Wrap
        }

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            visible: GilCoordinator.busy
            running: visible
        }

        Button {
            Layout.fillWidth: true
            text: qsTr("Sign in with GILid")
            enabled: !GilCoordinator.busy
            onClicked: GilCoordinator.startLogin()
        }

        Button {
            Layout.fillWidth: true
            text: qsTr("Skip login (development only)")
            visible: GilCoordinator.developmentBuild
            enabled: visible && !GilCoordinator.busy
            onClicked: GilCoordinator.skipLoginForDevelopment()
        }

        Button {
            Layout.fillWidth: true
            text: qsTr("Try again")
            visible: GilCoordinator.authenticated && !GilCoordinator.busy
            onClicked: GilCoordinator.requestVm()
        }
    }
}
