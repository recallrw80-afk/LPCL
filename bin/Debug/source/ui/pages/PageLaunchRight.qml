import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PCL.Core
import "../components"
import "../styles"

// Exact replica of PageLaunchRight.xaml
// Right content area for the launch page
Item {
    id: page

    Flickable {
        id: scrollView
        anchors.fill: parent
        contentWidth: width
        contentHeight: panMain.implicitHeight + 35
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: MyScrollBar {}

        ColumnLayout {
            id: panMain
            anchors { left: parent.left; right: parent.right; top: parent.top }
            anchors.margins: 25
            anchors.rightMargin: 25
            anchors.bottomMargin: 10
            spacing: 15

            // ---- Hint banner ----
            Rectangle {
                id: panHint
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                radius: Theme.buttonRadius
                color: Theme.color6
                border.color: Theme.color5

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 10

                    Text {
                        text: "ℹ"
                        color: Theme.color3
                        font.pixelSize: Theme.fontSizeLarge
                    }
                    Text {
                        text: "PCL_LIUNX v0.1 — Cross-platform Minecraft Launcher (Qt 6.8)"
                        color: Theme.color3
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                        Layout.fillWidth: true
                        elide: Text.ElideRight
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

            // ---- What's New card ----
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: cardContent.height + 40
                radius: Theme.buttonRadius
                color: Theme.pureWhite
                border.color: Theme.gray5

                ColumnLayout {
                    id: cardContent
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    anchors.margins: 25
                    anchors.topMargin: 40
                    spacing: 8

                    Text {
                        text: "PCL_LIUNX"
                        color: Theme.color1
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLaunchName
                        font.bold: true
                    }

                    Text {
                        text: "Cross-platform port of Plain Craft Launcher 2"
                        color: Theme.gray3
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                    }

                    Text {
                        text: "• Built with Qt 6.8 + QML + C++17\n• Supports Windows, macOS, Linux\n• Original PCL: 49,000+ lines VB.NET/WPF"
                        color: Theme.color1
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        Layout.topMargin: 4
                    }

                    RowLayout {
                        Layout.topMargin: 12
                        spacing: 10

                        MyButton {
                            text: "Source Code"
                            colorType: 1
                            onClicked: Qt.openUrlExternally("https://github.com")
                        }
                        MyButton {
                            text: "Report Bug"
                            colorType: 0
                            onClicked: Qt.openUrlExternally("https://github.com")
                        }
                    }
                }
            }

            // ---- Quick Links card ----
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: linksContent.height + 40
                radius: Theme.buttonRadius
                color: Theme.pureWhite
                border.color: Theme.gray5

                ColumnLayout {
                    id: linksContent
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    anchors.margins: 25
                    anchors.topMargin: 40
                    spacing: 6

                    Text {
                        text: "Quick Links"
                        color: Theme.color1
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLaunchName
                        font.bold: true
                    }

                    Repeater {
                        model: [
                            { text: "Minecraft Official Site", url: "https://minecraft.net" },
                            { text: "MC Versions (Mojang API)", url: "https://launchermeta.mojang.com" },
                            { text: "Modrinth", url: "https://modrinth.com" },
                            { text: "CurseForge", url: "https://curseforge.com/minecraft" }
                        ]

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 35
                            spacing: 10

                            Rectangle {
                                width: 6; height: 6; radius: 3
                                color: Theme.color3
                            }

                            Text {
                                text: modelData.text
                                color: Theme.color1
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSize
                                Layout.fillWidth: true
                            }

                            MyButton {
                                text: "Open"
                                Layout.preferredWidth: 50
                                Layout.preferredHeight: 26
                                colorType: 0
                                onClicked: Qt.openUrlExternally(modelData.url)
                            }
                        }
                    }
                }
            }

            // ---- Launch Log card ----
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: 120
                radius: Theme.buttonRadius
                color: Theme.pureWhite
                border.color: Theme.gray5

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 0
                    spacing: 0

                    Text {
                        text: "Launch Log"
                        color: Theme.color1
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLaunchName
                        font.bold: true
                        Layout.leftMargin: 25
                        Layout.topMargin: 15
                    }

                    Flickable {
                        id: logFlick
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.margins: 20
                        Layout.leftMargin: 20
                        Layout.rightMargin: 23
                        Layout.topMargin: 10
                        Layout.bottomMargin: 18
                        clip: true
                        contentWidth: width
                        contentHeight: logText.implicitHeight

                        ScrollBar.vertical: MyScrollBar {}

                        Text {
                            id: logText
                            width: parent.width
                            text: "PCL_LIUNX v0.1\nReady.\n"
                            color: Theme.color1
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize
                            wrapMode: Text.Wrap
                            textFormat: Text.PlainText
                        }
                    }
                }
            }
        }
    }

    // Connections for launch log
    Connections {
        target: Launcher
        function onGameLog(line) {
            logText.text += line + "\n"
        }
    }
}
