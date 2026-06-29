import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PCL.Core
import "../components"
import "../styles"

// Exact replica of PageLaunchRight.xaml
// Right content: MyScrollViewer > StackPanel(PanMain) > Hint + Card("启动日志") > LabLog
Item {
    id: page

    Flickable {
        id: panBack
        anchors.fill: parent
        contentWidth: width
        contentHeight: panMain.implicitHeight + 25
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: MyScrollBar {}

        ColumnLayout {
            id: panMain
            anchors { left: parent.left; right: parent.right; top: parent.top }
            anchors.margins: 25
            spacing: 15

            // ---- Hint banner (PanHint) — snapshot warning, Blue theme, closable ----
            Rectangle {
                id: panHint
                Layout.fillWidth: true
                Layout.preferredHeight: hintText.implicitHeight + 28
                radius: Theme.buttonRadius
                color: "transparent"
                border.width: 0
                visible: true
                clip: true

                // Left accent bar (Blue theme)
                Rectangle {
                    anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                    width: 4
                    color: Theme.color3
                    radius: 2
                }

                // Background
                Rectangle {
                    anchors { left: parent.left; right: parent.right; top: parent.top; bottom: parent.bottom }
                    anchors.leftMargin: 4
                    color: Theme.color6
                    radius: Theme.buttonRadius
                }

                RowLayout {
                    anchors { fill: parent; margins: 8; leftMargin: 14 }
                    spacing: 8

                    Text {
                        id: hintText
                        text: "快照版 PCL 包含尚未正式发布的测试功能。\n请不要随意发给其他人。"
                        color: Theme.color3
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }

                    MyButton {
                        text: "×"
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                        colorType: 0
                        onClicked: panHint.visible = false
                    }
                }
            }

            // ---- Custom content area (PanCustom) ----
            // Placeholder for user-customized homepage content
            Item {
                id: panCustom
                Layout.fillWidth: true
                Layout.preferredHeight: 0
            }

            // ---- Launch Log card (PanLog) — MyCard, Title="启动日志" ----
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 150
                radius: Theme.buttonRadius
                color: Theme.pureWhite
                border { width: 1; color: Theme.gray5 }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Card title
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        color: "transparent"

                        Text {
                            anchors { left: parent.left; top: parent.top; leftMargin: 20; topMargin: 18 }
                            text: "启动日志"
                            color: Theme.color1
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeLaunchName
                            font.bold: true
                        }
                    }

                    // Log text (LabLog)
                    Flickable {
                        id: logFlick
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.leftMargin: 20
                        Layout.rightMargin: 23
                        Layout.bottomMargin: 18
                        clip: true
                        contentWidth: width
                        contentHeight: labLog.implicitHeight
                        ScrollBar.vertical: MyScrollBar {}

                        Text {
                            id: labLog
                            width: parent.width
                            text: "LPCL v0.1\nReady.\n"
                            color: Theme.color1
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize
                            wrapMode: Text.WordWrap
                            textFormat: Text.PlainText
                        }
                    }
                }
            }
        }
    }

    // Connections for live log output
    Connections {
        target: Launcher
        function onGameLog(line) {
            labLog.text += line + "\n"
        }
    }
}
