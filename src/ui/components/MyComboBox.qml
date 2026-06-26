import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ComboBox {
    id: control
    property int radius: Theme.radiusMd
    property int fontSize: Theme.fontSizeMd

    implicitWidth: 180
    implicitHeight: 36

    delegate: ItemDelegate {
        width: control.popup.width
        contentItem: Text {
            text: modelData
            color: Theme.textPrimary
            font.pixelSize: control.fontSize
            elide: Text.ElideRight
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            color: hovered ? Theme.bgInput : "transparent"
            radius: Theme.radiusSm
        }
    }

    contentItem: Text {
        text: control.displayText
        font.pixelSize: control.fontSize
        color: Theme.textPrimary
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        leftPadding: Theme.spacingSm
    }

    background: Rectangle {
        implicitWidth: control.implicitWidth
        implicitHeight: control.implicitHeight
        radius: control.radius
        color: Theme.bgInput
        border.color: control.activeFocus ? Theme.accent : Theme.border
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight, 300)
        padding: 4

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            ScrollIndicator.vertical: ScrollIndicator {}
        }

        background: Rectangle {
            radius: Theme.radiusMd
            color: Theme.bgSecondary
            border.color: Theme.border
        }
    }
}
