import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components"
import "../styles"

// Download page placeholder — matching original PageDownload
Item {
    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: content.implicitHeight + 35
        clip: true
        ScrollBar.vertical: LPCLScrollBar {}

        ColumnLayout {
            id: content
            anchors { left: parent.left; right: parent.right; top: parent.top }
            anchors.margins: 25
            spacing: 15

            Text {
                text: "Download"
                color: Theme.color1
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLogo
                font.bold: true
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 100
                radius: Theme.buttonRadius
                color: Theme.pureWhite
                border.color: Theme.gray5

                ColumnLayout {
                    anchors.centerIn: parent
                    Text {
                        text: "Resource download will be available in Phase 4"
                        color: Theme.gray3
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                    }
                }
            }
        }
    }
}
