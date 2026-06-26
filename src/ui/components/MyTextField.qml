import QtQuick
import QtQuick.Controls.Basic

TextField {
    id: control
    property int radius: Theme.radiusMd
    property int fontSize: Theme.fontSizeMd
    property color placeholderColor: Theme.textDisabled

    implicitWidth: 200
    implicitHeight: 36

    font.pixelSize: control.fontSize
    color: Theme.textPrimary
    verticalAlignment: TextInput.AlignVCenter
    leftPadding: Theme.spacingSm
    rightPadding: Theme.spacingSm

    placeholderTextColor: control.placeholderColor

    background: Rectangle {
        implicitWidth: control.implicitWidth
        implicitHeight: control.implicitHeight
        radius: control.radius
        color: Theme.bgInput
        border.color: control.activeFocus ? Theme.accent : Theme.border

        Behavior on border.color {
            ColorAnimation { duration: 150 }
        }
    }
}
