import QtQuick 2.0
import QtQuick.Controls

import ComputerManager 1.0
import Session 1.0

import "theme"

Item {
    property string appName
    property var quitRunningAppFn
    property Session nextSession : null
    property string nextAppName : ""
    // 退出旧游戏后要接着启动的那个游戏的封面，转交给加载页当背景
    property string nextBoxArtUrl : ""

    property string stageText : qsTr("Quitting %1...").arg(appName)

    function quitAppCompleted(error)
    {
        // Display a failed dialog if we got an error
        if (error !== undefined) {
            errorDialog.text = error
            errorDialog.open()
            console.error(error)
        }

        // If we're supposed to launch another game after this, do so now
        if (error === undefined && nextSession !== null) {
            var component = Qt.createComponent("StreamSegue.qml")
            var segue = component.createObject(stackView, {"appName": nextAppName,
                                                                    "boxArtUrl": nextBoxArtUrl,
                                                                    "session": nextSession})
            stackView.replace(segue)
        }
        else {
            // Exit this view
            stackView.pop()
        }
    }

    StackView.onActivated: {
        // Hide the toolbar before we start loading
        toolBar.shown = false

        // Connect the quit completion signal
        ComputerManager.quitAppCompleted.connect(quitAppCompleted)

        // Start the quit operation if requested
        if (quitRunningAppFn) {
            quitRunningAppFn()
        }
    }

    StackView.onDeactivating: {
        // Show the toolbar again
        toolBar.shown = true

        // Disconnect the signal
        ComputerManager.quitAppCompleted.disconnect(quitAppCompleted)
    }

    // 和加载页同一套：Manrope 800 大字阶段文字 + 一条来回扫的酸性绿实心条，不用转圈
    Column {
        anchors.centerIn: parent
        width: Math.min(parent.width - Theme.spaceXl * 2, 620)
        spacing: Theme.spaceLg

        Text {
            id: stageLabel

            width: parent.width
            text: stageText
            color: Theme.text
            font.family: Theme.fontSans
            font.pointSize: 24
            font.weight: Font.ExtraBold
            font.letterSpacing: Theme.trackingTight(24)
            // 和加载页一致：咬着读条的左基线，不居中
            horizontalAlignment: Text.AlignLeft
            wrapMode: Text.Wrap
        }

        HardProgress {
            id: stageSpinner

            width: parent.width
        }
    }

    ErrorMessageDialog {
        id: errorDialog
    }
}
