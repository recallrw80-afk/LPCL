import QtQuick
import QtQuick.Controls.Basic
import "../styles"

// Exact replica of original MyButton (Controls/MyButton.xaml + .xaml.vb)
// Structure: PanBack(semiTransparent) > PanFore(border+halfWhite bg) > LabText
Item {
    id: wrapper

    // ---- Public API ----
    property alias text: labText.text
    property alias enabled: mouseArea.enabled
    property alias font: labText.font
    property int colorType: 0           // 0=Normal, 1=Highlight, 2=Red, 3=Disabled
    property int padding: 10
    property bool down: false
    property bool pressed: false
    property real radius: Theme.buttonRadius

    signal clicked()
    signal pressAndHold()

    implicitWidth: Math.max(60, labText.implicitWidth + padding * 2)
    implicitHeight: 28

    // ---- Color logic (exact match original code-behind) ----
    property color borderColor: {
        if (colorType === 3 || !enabled) return Theme.gray4
        if (hovered) {
            if (colorType === 2) return Theme.redLight
            return Theme.color3
        }
        if (colorType === 2) return Theme.redDark
        if (colorType === 1) return Theme.color2
        return Theme.color1
    }
    property color backgroundColor: {
        if (colorType === 3 || !enabled) return Theme.gray6
        if (hovered) {
            if (colorType === 2) return Theme.redBack
            if (colorType === 1) return Theme.color7
            return Theme.color7
        }
        return Theme.halfWhite
    }
    property bool hovered: false

    // ---- Press scale animation (exact match original ScaleTransform) ----
    property real btnScale: 1.0

    Behavior on btnScale {
        NumberAnimation { duration: 300; easing.type: Easing.OutBack }
    }

    transform: Scale {
        origin.x: wrapper.width / 2
        origin.y: wrapper.height / 2
        xScale: wrapper.btnScale
        yScale: wrapper.btnScale
    }

    // ---- PanBack (outer border, semiTransparent background) ----
    Rectangle {
        id: panBack
        anchors.fill: parent
        color: Theme.semiTransparent
        radius: wrapper.radius

        // ---- PanFore (inner border, colored border + halfWhite background) ----
        Rectangle {
            id: panFore
            anchors.fill: parent
            radius: wrapper.radius
            border.width: 1
            border.color: wrapper.borderColor
            color: wrapper.backgroundColor

            Behavior on border.color { ColorAnimation { duration: 100 } }
            Behavior on color { ColorAnimation { duration: 100 } }

            // ---- LabText (text color = border color, matching original) ----
            Text {
                id: labText
                anchors.centerIn: parent
                color: panFore.border.color
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSize
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
            }
        }
    }

    // ---- Mouse handling ----
    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor

        onEntered: wrapper.hovered = true
        onExited: { wrapper.hovered = false; wrapper.down = false; wrapper.pressed = false }
        onPressed: { wrapper.btnScale = 0.95; wrapper.down = true; wrapper.pressed = true }
        onReleased: { wrapper.btnScale = 1.0; wrapper.down = false; wrapper.pressed = false }
        onCanceled: { wrapper.btnScale = 1.0; wrapper.down = false; wrapper.pressed = false }
        onClicked: wrapper.clicked()
        onPressAndHold: wrapper.pressAndHold()
    }
}
