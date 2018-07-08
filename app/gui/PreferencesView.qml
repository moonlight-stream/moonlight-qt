import QtQuick 2.9
Rectangle {
    id: preferencesPage
    color: "#333333"

    Text {
        id: preferencesTitleText
        text: "Preferences"
        color: "white"

        width: parent.width
        font.pointSize: 36
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
    }
}
