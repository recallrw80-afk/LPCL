import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "../components"
import "../styles"

Page {
    id: page

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingMd
        spacing: Theme.spacingMd

        Text {
            text: qsTr("Settings")
            font.pixelSize: Theme.fontSizeXl
            font.bold: true
            color: Theme.textPrimary
        }

        // Minecraft folder
        GroupBox {
            title: qsTr("Minecraft Folder")
            Layout.fillWidth: true
            background: Rectangle {
                radius: Theme.radiusMd
                color: Theme.bgCard
                border.color: Theme.border
            }

            RowLayout {
                spacing: Theme.spacingSm

                MyTextField {
                    id: mcFolderField
                    Layout.fillWidth: true
                    text: ""
                    placeholderText: qsTr("Select Minecraft folder...")
                    readOnly: true
                }

                MyButton {
                    text: qsTr("Browse...")
                    bgColor: Theme.bgInput
                    bgHover: Theme.border
                    textColor: Theme.textPrimary
                }
            }
        }

        // Java settings
        GroupBox {
            title: qsTr("Java")
            Layout.fillWidth: true
            background: Rectangle {
                radius: Theme.radiusMd
                color: Theme.bgCard
                border.color: Theme.border
            }

            ColumnLayout {
                spacing: Theme.spacingSm

                MyButton {
                    text: qsTr("Scan for Java")
                    Layout.fillWidth: true
                    onClicked: {
                        // Will call JavaManager.scanSystemJava()
                    }
                }
            }
        }

        // About
        GroupBox {
            title: qsTr("About")
            Layout.fillWidth: true
            background: Rectangle {
                radius: Theme.radiusMd
                color: Theme.bgCard
                border.color: Theme.border
            }

            ColumnLayout {
                Label { text: "PCL_LIUNX v0.1"; font.bold: true; color: Theme.textPrimary; font.pixelSize: Theme.fontSizeLg }
                Label { text: qsTr("Cross-platform Minecraft Launcher"); color: Theme.textSecondary }
                Label { text: qsTr("Original PCL: Plain Craft Launcher 2"); color: Theme.textSecondary }
                Label { text: qsTr("Qt 6.8 + QML + C++17"); color: Theme.textSecondary }
            }
        }

        // Spacer
        Item { Layout.fillHeight: true }
    }
}
