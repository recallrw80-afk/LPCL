import QtQuick
import Qt5Compat.GraphicalEffects
import "../styles"

// Minimal icon display: SVG source + ColorOverlay tinting
Item {
    id: wrapper

    // ---- Public API ----
    property string iconSource: ""     // QRC path (e.g. "qrc:/assets/icons/xxx.svg")
    property string lucideIcon: ""     // Lucide icon name (e.g. "play") → auto-resolves
    property color iconColor: Theme.color3

    implicitWidth: 28
    implicitHeight: 28

    readonly property url _resolvedSource: lucideIcon !== ""
        ? "qrc:/assets/icons/lucide/" + lucideIcon + ".svg"
        : iconSource

    Image {
        id: iconImage
        anchors.fill: parent
        source: wrapper._resolvedSource
        sourceSize: Qt.size(implicitWidth * 4, implicitHeight * 4)
        smooth: true
        mipmap: true
        visible: false
    }
    ColorOverlay {
        anchors.fill: iconImage
        source: iconImage
        color: wrapper.iconColor
    }
}
