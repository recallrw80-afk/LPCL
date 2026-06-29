import QtQuick
import QtQuick.Controls.Basic
import "../styles"

// Exact replica of original MyButton (Controls/MyButton.xaml + .xaml.vb)
// Uses a wrapper Item to avoid conflicting with Button's FINAL properties
Item {
    id: wrapper

    property alias text: btn.text
    property alias enabled: btn.enabled
    property alias down: btn.down
    property alias pressed: btn.pressed

    // Color types matching original: 0=Normal, 1=Highlight, 2=Red, 3=Disabled
    property int colorType: 0
    property color customBorderColor: Theme.color1
    property color textColor: borderColor
    property int logoScale: 1

    implicitWidth: Math.max(60, btn.contentItem.implicitWidth + 20)
    implicitHeight: 28

    // Click signal passthrough
    signal clicked()
    signal pressAndHold()

    // Determine colors by state + type
    property color borderColor: {
        if (!btn.enabled) return Theme.gray4
        switch (colorType) {
            case 1: return btn.hovered ? Theme.color3 : Theme.color2  // Highlight
            case 2: return btn.hovered ? Theme.redLight : Theme.redDark  // Red
            default: return btn.hovered ? Theme.color3 : Theme.color1  // Normal
        }
    }
    property color bgColor: {
        if (!btn.enabled) return Theme.gray6
        if (btn.hovered) {
            return colorType === 2 ? Theme.redBack : Theme.color7
        }
        return Theme.halfWhite
    }

    // Scale animation on press (matches original click behavior)
    // Use transform instead of scale since Item.scale is FINAL in Qt6
    property real btnScale: 1.0

    transform: Scale {
        origin.x: wrapper.width / 2
        origin.y: wrapper.height / 2
        xScale: wrapper.btnScale
        yScale: wrapper.btnScale
    }

    Behavior on btnScale {
        NumberAnimation { duration: 300; easing.type: Easing.OutBack }
    }

    Button {
        id: btn
        anchors.fill: parent

        // Remove default styling
        flat: true
        padding: 10

        contentItem: Text {
            text: btn.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize
            color: wrapper.textColor
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            id: panBack
            color: Theme.semiTransparent
            radius: Theme.buttonRadius

            Rectangle {
                id: panFore
                anchors.fill: parent
                radius: Theme.buttonRadius
                border.width: 1
                border.color: wrapper.borderColor
                color: wrapper.bgColor

                Behavior on border.color { ColorAnimation { duration: 100 } }
                Behavior on color { ColorAnimation { duration: 100 } }
            }
        }

        // Handle press animation via wrapper
        onPressed: {
            wrapper.btnScale = 0.95
        }
        onReleased: {
            wrapper.btnScale = 1.0
        }
        onCanceled: {
            wrapper.btnScale = 1.0
        }

        onClicked: wrapper.clicked()
        onPressAndHold: wrapper.pressAndHold()
    }

    // Expose commonly needed properties
    property alias font: btn.font
}
