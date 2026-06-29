import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../styles"

// Download tab — left sidebar + right content
RowLayout {
    anchors.fill: parent
    spacing: 0

    Rectangle {
        Layout.preferredWidth: 300
        Layout.fillHeight: true
        color: Theme.sidebarBg
        Rectangle {
            anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
            width: 4; opacity: 0.04
            gradient: Gradient {
                GradientStop { position: 0; color: "#000000" }
                GradientStop { position: 1; color: "#00000000" }
            }
        }
        Text {
            anchors.centerIn: parent
            text: "下载"
            color: Theme.gray3
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLarge
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        color: "transparent"
        Text {
            anchors.centerIn: parent
            text: "下载页面"
            color: Theme.gray3
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLarge
        }
    }
}
