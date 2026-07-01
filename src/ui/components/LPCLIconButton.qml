import QtQuick
import Qt5Compat.GraphicalEffects
import "../styles"

// Exact replica of original LPCLIconButton (Controls/LPCLIconButton.xaml + .xaml.vb)
// Theme: Color (theme-based), White, Black, Red
// Icons are external SVG files (not inline paths — see RED LINE in CLAUDE.md)
Item {
    id: wrapper

    // ---- Public API ----
    property url iconSource: ""           // SVG file path (e.g. "qrc:/assets/icons/xxx.svg")
    property real logoScale: 1.0
    property string theme: "Color"       // "Color" | "White" | "Black" | "Red"
    property bool enabled: true

    signal clicked()
    signal rightClicked()

    implicitWidth: 28
    implicitHeight: 28

    // ---- Derived properties ----
    property bool hovered: false
    property bool pressed: false

    property color iconColor: {
        switch (theme) {
            case "White": return "#ffffff"
            case "Black": return "#000000"
            case "Red":   return Theme.redDark
            default:      return Theme.color3  // "Color" → theme highlight
        }
    }
    property color hoverBg: {
        switch (theme) {
            case "White": return "#33ffffff"
            case "Black": return "#11000000"
            case "Red":   return Theme.redBack
            default:      return "#11000000"
        }
    }

    // ---- Press scale animation ----
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

    // ---- Hover circle background ----
    Rectangle {
        anchors.centerIn: parent
        width: Math.min(parent.width, parent.height) - 4
        height: width
        radius: width / 2
        color: wrapper.hovered ? wrapper.hoverBg : "transparent"
        Behavior on color { ColorAnimation { duration: 100 } }
    }

    // ---- SVG icon from external file (Image + ColorOverlay, same pattern as nav tab icons) ----
    property real iconSize: Math.min(parent.width, parent.height) * 0.72 * wrapper.logoScale

    Image {
        id: iconImage
        anchors.centerIn: parent
        width: wrapper.iconSize
        height: wrapper.iconSize
        source: wrapper.iconSource
        sourceSize: Qt.size(iconSize * 4, iconSize * 4)
        smooth: true
        mipmap: true
        visible: false
    }
    ColorOverlay {
        anchors.fill: iconImage
        source: iconImage
        color: wrapper.iconColor
    }

    // ---- Mouse handling ----
    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        enabled: wrapper.enabled

        onEntered: wrapper.hovered = true
        onExited: { wrapper.hovered = false; wrapper.pressed = false }
        onPressed: (mouse) => { wrapper.btnScale = 0.9; wrapper.pressed = true }
        onReleased: { wrapper.btnScale = 1.0; wrapper.pressed = false }
        onCanceled: { wrapper.btnScale = 1.0; wrapper.pressed = false }
        onClicked: (mouse) => wrapper.clicked()
        onPressAndHold: (mouse) => wrapper.rightClicked()
    }
}
