import QtQuick
import QtQuick.Controls.Basic

Button {
    id: control
    property color bgColor: Theme.accent
    property color bgHover: Theme.accentHover
    property color textColor: "white"
    property int radius: Theme.radiusMd
    property int fontSize: Theme.fontSizeMd
    property bool isPrimary: true

    implicitWidth: 100
    implicitHeight: 36

    contentItem: Text {
        text: control.text
        font.pixelSize: control.fontSize
        color: control.textColor
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        implicitWidth: control.implicitWidth
        implicitHeight: control.implicitHeight
        radius: control.radius
        color: control.hovered ? control.bgHover : control.bgColor

        Behavior on color {
            ColorAnimation { duration: 150 }
        }
    }
}
