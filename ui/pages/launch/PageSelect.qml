import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import LPCL

// Replica of PageSelectLeft.xaml — Minecraft folder selection sidebar
// AnimatedControl="PanList" with staggered slide-in
Item {
    id: page

    Flickable {
        id: scrollView
        anchors.fill: parent
        contentWidth: width
        contentHeight: panList.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: LPCLScrollBar {}

        ColumnLayout {
            id: panList
            anchors { left: parent.left; right: parent.right; top: parent.top }
            spacing: 0

            // ---- Section: Minecraft Folders ----
            Text {
                text: "Minecraft Folders"
                color: Theme.color1
                opacity: 0.6
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
                Layout.leftMargin: 13
                Layout.topMargin: 18
                Layout.bottomMargin: 4
            }

            // Folder items (height=40)
            Repeater {
                model: page.folderItems

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.folderItemHeight
                    color: mouseArea.containsMouse ? Theme.color7 : "transparent"
                    radius: Theme.buttonRadius
                    Layout.leftMargin: 5
                    Layout.rightMargin: 5

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        spacing: 10

                        Text {
                                            // qmllint disable unqualified
                            text: modelData.name
                            color: Theme.color1
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                        }

                        LPCLButton {
                            text: "⚙"
                            Layout.preferredWidth: 28
                            Layout.preferredHeight: 28
                            colorType: 0
                        }
                    }

                    MouseArea {
                        id: mouseArea
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: {
                                            // qmllint disable unqualified
                            VersionManager.mcFolder = modelData.location
                        }
                    }
                }
            }

            // Action items (height=34)
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.actionItemHeight
                color: mouseArea1.containsMouse ? Theme.color7 : "transparent"
                radius: Theme.buttonRadius
                Layout.leftMargin: 5
                Layout.rightMargin: 5

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    Text {
                        text: "+ New .minecraft Folder"
                        color: Theme.color1
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                    }
                }

                MouseArea {
                    id: mouseArea1
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: { /* Create new folder */ }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.actionItemHeight
                color: mouseArea2.containsMouse ? Theme.color7 : "transparent"
                radius: Theme.buttonRadius
                Layout.leftMargin: 5
                Layout.rightMargin: 5

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    Text {
                        text: "+ Add Existing Folder"
                        color: Theme.color1
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                    }
                }

                MouseArea {
                    id: mouseArea2
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: { /* Add existing folder */ }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.actionItemHeight
                color: mouseArea3.containsMouse ? Theme.color7 : "transparent"
                radius: Theme.buttonRadius
                Layout.leftMargin: 5
                Layout.rightMargin: 5

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    Text {
                        text: "↓ Import Modpack"
                        color: Theme.color1
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSize
                    }
                }

                MouseArea {
                    id: mouseArea3
                    anchors.fill: parent
                    hoverEnabled: true
                    onClicked: { /* Import modpack */ }
                }
            }

            // Bottom spacer
            Item { Layout.preferredHeight: 10 }
        }
    }

    // Dynamic folder items
    property var folderItems: [
        { name: ".minecraft", location: "" },
        { name: "No folders detected", location: "" }
    ]

    function pageOnEnter() {
        // Refresh folder list
    }
}
