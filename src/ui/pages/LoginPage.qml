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
            text: qsTr("Login Settings")
            font.pixelSize: Theme.fontSizeXl
            font.bold: true
            color: Theme.textPrimary
        }

        // Login type selector
        GroupBox {
            title: qsTr("Login Method")
            Layout.fillWidth: true
            background: Rectangle {
                radius: Theme.radiusMd
                color: Theme.bgCard
                border.color: Theme.border
            }

            ColumnLayout {
                spacing: Theme.spacingSm

                Repeater {
                    model: [
                        { text: qsTr("Offline (Legacy)"), type: 0 },
                        { text: qsTr("Microsoft (OAuth)"), type: 5 },
                        { text: qsTr("Authlib-Injector"), type: 3 },
                        { text: qsTr("Nide8 / Unified Pass"), type: 2 }
                    ]

                    RadioButton {
                        text: modelData.text
                        checked: index === 0 // Default offline
                        font.pixelSize: Theme.fontSizeMd
                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: Theme.textPrimary
                            leftPadding: parent.indicator.width + parent.spacing
                        }
                    }
                }
            }
        }

        // Microsoft login section
        GroupBox {
            title: qsTr("Microsoft Login")
            Layout.fillWidth: true
            visible: false // Visible when MS login selected
            background: Rectangle {
                radius: Theme.radiusMd
                color: Theme.bgCard
                border.color: Theme.border
            }

            ColumnLayout {
                spacing: Theme.spacingSm

                Label {
                    text: qsTr("Click to start Microsoft OAuth login.\nA browser window will open for you to sign in.")
                    color: Theme.textSecondary
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }

                MyButton {
                    text: qsTr("Microsoft Login")
                    Layout.fillWidth: true
                }
            }
        }
    }
}
