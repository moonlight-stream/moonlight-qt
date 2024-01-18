import QtQuick 2.0
import QtQuick.Controls 2.2

ItemDelegate {
    property Flickable grid

    highlighted: grid.activeFocus && grid.currentItem === this

    Keys.onLeftPressed: {
        if (grid instanceof GridView) {
            grid.moveCurrentIndexLeft()
        }else if(grid instanceof ListView){
            grid.decrementCurrentIndex()
        }
    }
    Keys.onRightPressed: {
        if (grid instanceof GridView) {
            grid.moveCurrentIndexRight()
        }else if(grid instanceof ListView){
            grid.incrementCurrentIndex()
        }
    }
    Keys.onDownPressed: {
        grid.moveCurrentIndexDown()
    }
    Keys.onUpPressed: {
        grid.moveCurrentIndexUp()
    }
    Keys.onReturnPressed: {
        clicked()
    }
    Keys.onEnterPressed: {
        clicked()
    }
}
