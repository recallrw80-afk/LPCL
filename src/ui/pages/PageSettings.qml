import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PCL.Core
import "../components"
import "../styles"

// Settings tab — left sidebar + right content
Item {
    property bool isActive: false
    visible: opacity > 0
    opacity: isActive ? 1 : 0
    scale: isActive ? 1 : 0.96
    Behavior on opacity { NumberAnimation { duration: 100 } }
    Behavior on scale { NumberAnimation { duration: 400; easing.type: Easing.OutBack } }

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
            text: "设置"
            color: Theme.gray3
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSizeLarge
        }
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        color: "transparent"

        Flickable {
            id: scrollView
            anchors.fill: parent
            contentWidth: width
            contentHeight: panContent.implicitHeight + 35
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: LPCLScrollBar {}

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

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: launchGrid.height + 40
                    radius: Theme.buttonRadius
                    color: Theme.pureWhite
                    border.color: Theme.gray5
                    ColumnLayout {
                        anchors { left: parent.left; right: parent.right; top: parent.top }
                        anchors.margins: 25; anchors.topMargin: 40
                        spacing: 10
                        Text { text: "Launch Settings"; color: Theme.color1; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeLarge; font.bold: true }
                        GridLayout {
                            id: launchGrid
                            columns: 2; rowSpacing: 8; columnSpacing: 15
                            Text { text: "Max Memory:"; color: Theme.color1; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                            LPCLTextBox { id: maxMemField; text: "4096"; Layout.preferredWidth: 120; placeholderText: "MB" }
                            Text { text: "Window Size:"; color: Theme.color1; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                            RowLayout {
                                LPCLTextBox { id: widthField; text: "854"; Layout.preferredWidth: 70; placeholderText: "W" }
                                Text { text: "×"; color: Theme.gray3 }
                                LPCLTextBox { id: heightField; text: "480"; Layout.preferredWidth: 70; placeholderText: "H" }
                            }
                            Text { text: "Java Args:"; color: Theme.color1; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                            LPCLTextBox { id: javaArgsField; text: ""; Layout.fillWidth: true; placeholderText: "Custom JVM arguments" }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: javaContent.height + 40
                    radius: Theme.buttonRadius
                    color: Theme.pureWhite
                    border.color: Theme.gray5
                    ColumnLayout {
                        id: javaContent
                        anchors { left: parent.left; right: parent.right; top: parent.top }
                        anchors.margins: 25; anchors.topMargin: 40
                        spacing: 10
                        Text { text: "Java Runtime"; color: Theme.color1; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeLarge; font.bold: true }
                        RowLayout {
                            Text { text: "Selected: " + JavaManager.selectedJavaName; color: Theme.gray3; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize; Layout.fillWidth: true }
                            LPCLButton { text: "Scan for Java"; colorType: 1; onClicked: JavaManager.scanSystemJava() }
                        }
                        Text { text: "Found " + JavaManager.javaCount + " Java installation(s)"; color: Theme.gray3; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSmall }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: uiContent.height + 40
                    radius: Theme.buttonRadius
                    color: Theme.pureWhite
                    border.color: Theme.gray5
                    ColumnLayout {
                        id: uiContent
                        anchors { left: parent.left; right: parent.right; top: parent.top }
                        anchors.margins: 25; anchors.topMargin: 40
                        spacing: 10
                        Text { text: "UI Settings"; color: Theme.color1; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeLarge; font.bold: true }
                        RowLayout {
                            Text { text: "Theme Color:"; color: Theme.color1; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                            LPCLComboBox { model: ["Blue (Default)", "Red", "Green", "Purple", "Orange"]; Layout.preferredWidth: 180 }
                        }
                        RowLayout {
                            Text { text: "Background Music:"; color: Theme.color1; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize }
                            LPCLComboBox { model: ["Off", "On"]; Layout.preferredWidth: 120 }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: aboutContent.height + 40
                    radius: Theme.buttonRadius
                    color: Theme.pureWhite
                    border.color: Theme.gray5
                    ColumnLayout {
                        id: aboutContent
                        anchors { left: parent.left; right: parent.right; top: parent.top }
                        anchors.margins: 25; anchors.topMargin: 40
                        spacing: 8
                        Text { text: "About LPCL"; color: Theme.color1; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeLarge; font.bold: true }
                        Text {
                            text: "Version 0.1 (MVP)\nBuilt with Qt 6.8 + QML + C++17\n\nPort of Plain Craft Launcher 2\nOriginal: ~49,000 lines VB.NET/WPF\n\nTarget: Windows, macOS, Linux"
                            color: Theme.gray3; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize; Layout.fillWidth: true; wrapMode: Text.Wrap
                        }
                    }
                }

                Item { Layout.preferredHeight: 10 }
            }
        }
    }
}
}
