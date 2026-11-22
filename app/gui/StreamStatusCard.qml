import QtQuick 2.9
import QtQuick.Controls 2.2
import QtQuick.Layouts 1.2

Rectangle {
    id: card
    property alias text: status.text
    property alias hint: detail.text
    property bool spinning: true
    property int step: 0
    property int stepCount: 0
    anchors.centerIn: parent
    width: Math.max(1, Math.min(460, parent.width - 32))
    height: contents.implicitHeight + 40
    color: "white"; radius: 10; border.color: "#dce4ec"
    ColumnLayout {
        id: contents
        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter; margins: 20 }
        spacing: 12
        Label {
            visible: card.stepCount > 0
            text: qsTr("Step %1 of %2").arg(card.step).arg(card.stepCount)
            color: "#385e8a"; font.pixelSize: 13; font.weight: Font.DemiBold
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            BusyIndicator {
                Layout.preferredWidth: 32; Layout.preferredHeight: 32
                visible: card.spinning; running: visible
            }
            Label {
                id: status
                Layout.fillWidth: true
                color: "#20364e"; font.pixelSize: 18; font.bold: true
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
            }
        }
        Label {
            id: detail
            Layout.fillWidth: true
            visible: text.length > 0
            color: "#52647a"; font.pixelSize: 14; wrapMode: Text.Wrap
        }
    }
}
