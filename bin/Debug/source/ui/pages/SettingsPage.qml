import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PCL.Core
import "../components"
import "../styles"

// Settings page — matching original PageSetup structure
Item {
    id: page

    Flickable {
        id: scrollView
        anchors.fill: parent
        contentWidth: width
        contentHeight: panContent.implicitHeight + 35
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: MyScrollBar {}

        ColumnLayout {
            id: panContent
            anchors { left: parent.left; right: parent.right; top: parent.top }
            anchors.margins: 25
            spacing: 15

            Text {
                text: "Settings"
                color: Theme.color1
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLogo
                font.bold: true
                Layout.bottomMargin: 10
            }

            // ---- Launch Settings ----
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: launchGrid.height + 40
                radius: Theme.buttonRadius
                color: Theme.pureWhite
                border.color: Theme.gray5

                ColumnLayout {
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    anchors.margins: 25
                    anchors.topMargin: 40
                    spacing: 10

                    Text {
                        text: "Launch Settings"
                        color: Theme.color1
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLarge
                        font.bold: true
                    }

                    GridLayout {
                        id: launchGrid
                        columns: 2
                        rowSpacing: 8
                        columnSpacing: 15

                        Text { text: "Max Memory:"; color: Theme.color1; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                        MyTextBox { id: maxMemField; text: "4096"; Layout.preferredWidth: 120; placeholderText: "MB" }

                        Text { text: "Window Size:"; color: Theme.color1; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                        RowLayout {
                            MyTextBox { id: widthField; text: "854"; Layout.preferredWidth: 70; placeholderText: "W" }
                            Text { text: "×"; color: Theme.gray3 }
                            MyTextBox { id: heightField; text: "480"; Layout.preferredWidth: 70; placeholderText: "H" }
                        }

                        Text { text: "Java Args:"; color: Theme.color1; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                        MyTextBox { id: javaArgsField; text: ""; Layout.fillWidth: true; placeholderText: "Custom JVM arguments" }
                    }
                }
            }

            // ---- Java Settings ----
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: javaContent.height + 40
                radius: Theme.buttonRadius
                color: Theme.pureWhite
                border.color: Theme.gray5

                ColumnLayout {
                    id: javaContent
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    anchors.margins: 25
                    anchors.topMargin: 40
                    spacing: 10

                    Text {
                        text: "Java Runtime"
                        color: Theme.color1
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLarge
                        font.bold: true
                    }

                    RowLayout {
                        Text {
                            text: "Selected: " + JavaManager.selectedJavaName
                            color: Theme.gray3
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize
                            Layout.fillWidth: true
                        }
                        MyButton {
                            text: "Scan for Java"
                            colorType: 1
                            onClicked: JavaManager.scanSystemJava()
                        }
                    }

                    Text {
                        text: "Found " + JavaManager.javaCount + " Java installation(s)"
                        color: Theme.gray3
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeSmall
                    }
                }
            }

            // ---- UI Settings ----
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: uiContent.height + 40
                radius: Theme.buttonRadius
                color: Theme.pureWhite
                border.color: Theme.gray5

                ColumnLayout {
                    id: uiContent
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    anchors.margins: 25
                    anchors.topMargin: 40
                    spacing: 10

                    Text {
                        text: "UI Settings"
                        color: Theme.color1
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLarge
                        font.bold: true
                    }

                    RowLayout {
                        Text { text: "Theme Color:"; color: Theme.color1; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                        MyComboBox {
                            model: ["Blue (Default)", "Red", "Green", "Purple", "Orange"]
                            Layout.preferredWidth: 180
                        }
                    }

                    RowLayout {
                        Text { text: "Background Music:"; color: Theme.color1; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                        MyComboBox {
                            model: ["Off", "On"]
                            Layout.preferredWidth: 120
                        }
                    }
                }
            }

            // ---- About ----
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: aboutContent.height + 40
                radius: Theme.buttonRadius
                color: Theme.pureWhite
                border.color: Theme.gray5

                ColumnLayout {
                    id: aboutContent
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    anchors.margins: 25
                    anchors.topMargin: 40
                    spacing: 8

                    Text {
                        text: "About PCL_LIUNX"
                        color: Theme.color1
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLarge
                        font.bold: true
                    }

                    Text {
                        text: "Version 0.1 (MVP)\nBuilt with Qt 6.8 + QML + C++17\n\nPort of Plain Craft Launcher 2\nOriginal: ~49,000 lines VB.NET/WPF\n\nTarget: Windows, macOS, Linux"
                        color: Theme.gray3
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                    }
                }
            }

            Item { Layout.preferredHeight: 10 }
        }
    }
}
