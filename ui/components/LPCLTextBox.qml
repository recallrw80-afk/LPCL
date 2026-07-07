import QtQuick
import QtQuick.Controls.Basic
import "../styles"

// MyTextBox — wraps TextField to avoid Qt6 FINAL property conflicts
Item {
    id: wrapper

    property alias text: field.text
    property alias placeholderText: field.placeholderText
    property alias readOnly: field.readOnly
    property alias validator: field.validator
    property alias echoMode: field.echoMode

    property string hintText: ""
    property bool hasError: false
    property string errorText: ""

    implicitWidth: 200
    implicitHeight: 28

    // Dynamic border color
    property color borderColor: {
        if (!field.enabled) return Theme.gray5
        if (hasError) return Theme.redLight
        if (field.activeFocus) return Theme.color3
        if (field.hovered) return Theme.color4
        return Theme.colorBg0
    }
    property color bgColor: {
        if (!field.enabled) return Theme.gray6
        if (hasError) return Theme.redBack
        if (field.activeFocus || field.hovered) return Theme.color7
        return Theme.halfWhite
    }

    // Background
    Rectangle {
        anchors.fill: parent
        radius: Theme.inputRadius
        border.width: 1
        border.color: wrapper.borderColor
        color: wrapper.bgColor

        Behavior on border.color { ColorAnimation { duration: wrapper.hasError || field.activeFocus ? 10 : 100 } }
        Behavior on color { ColorAnimation { duration: 100 } }

        // Hint text (placeholder overlay)
        Text {
            anchors.fill: parent
            anchors.leftMargin: 7
            verticalAlignment: Text.AlignVCenter
            text: !field.text && !field.activeFocus ? wrapper.hintText : ""
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSize
            color: Theme.gray4
            visible: !field.text && !field.activeFocus && wrapper.hintText !== ""
        }
    }

    // The actual TextField (styled minimally)
    TextField {
        id: field
        anchors.fill: parent
        enabled: wrapper.enabled   // follow Item.enabled instead of shadowing it
        anchors.leftMargin: 7
        anchors.rightMargin: 7

        font.family: Theme.fontFamily
        font.pixelSize: Theme.fontSize
        color: enabled ? Theme.color1 : Theme.gray4
        verticalAlignment: TextInput.AlignVCenter
        selectionColor: Theme.color3
        selectedTextColor: Theme.pureWhite

        // Transparent background — the wrapper Rectangle provides the visual style
        background: Item {}
    }
}
