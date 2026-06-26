import QtQuick
import QtQuick.Controls.Basic

ProgressBar {
    id: control
    property int barHeight: 6
    property color fillColor: Theme.accent

    implicitWidth: 200
    implicitHeight: barHeight

    background: Rectangle {
        implicitWidth: control.implicitWidth
        implicitHeight: control.barHeight
        radius: control.barHeight / 2
        color: Theme.bgInput
    }

    contentItem: Item {
        implicitWidth: control.implicitWidth
        implicitHeight: control.barHeight

        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: control.barHeight / 2
            color: control.fillColor

            Behavior on width {
                NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
            }
        }
    }
}
