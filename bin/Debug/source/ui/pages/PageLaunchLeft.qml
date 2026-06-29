import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PCL.Core
import "../components"
import "../styles"

// Exact replica of PageLaunchLeft.xaml
// Left sidebar for the launch page
Item {
    id: page

    // Normal state (PanInput)
    Item {
        id: panInput
        anchors.fill: parent
        opacity: 1

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 0
            spacing: 0

            // ---- Login type indicator (PanTypeOne) ----
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 27
                Layout.minimumHeight: 27
                Layout.maximumHeight: 27
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.topMargin: 24
                Layout.bottomMargin: -4
                radius: 13
                color: Theme.semiTransparent

                Row {
                    anchors.centerIn: parent
                    spacing: 0

                    // Icon
                    Canvas {
                        width: 16; height: 16
                        anchors.verticalCenter: parent.verticalCenter
                        scale: 1.05
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.fillStyle = Theme.color3
                            ctx.beginPath()
                            ctx.arc(8, 8, 7, 0, Math.PI * 2)
                            ctx.fill()
                        }
                    }

                    Text {
                        text: loginTypeText()
                        color: Theme.color3
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLarge
                        leftPadding: 15
                        rightPadding: 12
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            // Spacer
            Item { Layout.fillHeight: true }

            // ---- Login type selector (PanType) ----
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 35
                Layout.leftMargin: 20
                Layout.rightMargin: 20

                Item { Layout.fillWidth: true }
                MyButton {
                    text: "Microsoft"
                    colorType: loginType === 5 ? 1 : 0
                    Layout.preferredWidth: 80
                    onClicked: loginType = 5
                }
                Item { Layout.preferredWidth: 10 }
                MyButton {
                    text: "Offline"
                    colorType: loginType === 0 ? 1 : 0
                    Layout.preferredWidth: 70
                    onClicked: loginType = 0
                }
                Item { Layout.fillWidth: true }
            }

            Item { Layout.preferredHeight: 15 }

            // ---- Launch button (BtnLaunch) ----
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 61
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                color: "transparent"

                MyButton {
                    id: btnLaunch
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    height: Theme.launchBtnHeight
                    text: Launcher.isRunning ? "Running..." : "Launch"
                    colorType: 1 // Highlight
                    font.pixelSize: Theme.fontSizeLogo
                    enabled: !Launcher.isRunning

                    onClicked: {
                        // Switch to launching state
                        panInput.opacity = 0
                        panLaunching.visible = true
                        panLaunching.opacity = 1

                        // Launch logic (wired in later phase)
                        statusLabel.text = "Launching..."
                    }
                }

                // Version label under launch button
                Text {
                    id: versionLabel
                    anchors { horizontalCenter: parent.horizontalCenter; top: btnLaunch.bottom; topMargin: 2 }
                    text: versionCombo.currentText || "Select Version"
                    color: Theme.gray3
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeXSmall
                }
            }

            // ---- Version controls (Row 3) ----
            RowLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.versionBtnHeight + 10
                Layout.leftMargin: 0
                Layout.rightMargin: 10
                Layout.topMargin: 10

                MyComboBox {
                    id: versionCombo
                    Layout.fillWidth: true
                    Layout.preferredHeight: Theme.versionBtnHeight
                    model: VersionManager.versionIds.length > 0 ? VersionManager.versionIds : ["No versions found"]
                    enabled: !Launcher.isRunning
                }

                MyButton {
                    text: "..."
                    Layout.preferredWidth: 40
                    Layout.preferredHeight: Theme.versionBtnHeight
                    Layout.leftMargin: 10
                    enabled: !Launcher.isRunning
                    onClicked: {
                        // Version settings
                    }
                }
            }

            // Bottom spacer
            Item { Layout.preferredHeight: 20 }
        }
    }

    // ---- Launching state (PanLaunching) ----
    Item {
        id: panLaunching
        anchors.fill: parent
        visible: false
        opacity: 0

        Behavior on opacity { NumberAnimation { duration: 200 } }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 0
            spacing: 0

            Item { Layout.fillHeight: true }

            // Progress indicator
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 5

                // Spinning loading indicator
                Rectangle {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.preferredWidth: 50
                    Layout.preferredHeight: 50
                    Layout.bottomMargin: 5
                    radius: 25
                    color: Theme.color3
                    opacity: 0.15
                }

                // Title
                Text {
                    id: launchTitle
                    Layout.alignment: Qt.AlignHCenter
                    text: "Launching..."
                    color: Theme.color3
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLaunchTitle
                }

                // Version name
                Text {
                    id: launchName
                    Layout.alignment: Qt.AlignHCenter
                    Layout.leftMargin: 40
                    Layout.rightMargin: 40
                    Layout.topMargin: 5
                    text: versionCombo.currentText || ""
                    color: Theme.color3
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSizeLaunchName
                }

                // Progress bar
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 4
                    Layout.leftMargin: 30
                    Layout.rightMargin: 30
                    Layout.topMargin: 12
                    Layout.bottomMargin: 27
                    radius: 0
                    color: Qt.rgba(213/255, 230/255, 253/255, 0.6)

                    Rectangle {
                        width: parent.width * 0.6
                        height: parent.height
                        gradient: Gradient {
                            GradientStop { position: 0; color: Theme.color4 }
                            GradientStop { position: 1; color: Theme.color3 }
                        }
                    }
                }

                // Status info
                GridLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 25
                    Layout.rightMargin: 25
                    columns: 2
                    rowSpacing: 3
                    columnSpacing: 8

                    // Category labels (right-aligned, 0.5 opacity)
                    Text { text: "Step:"; color: Theme.color1; opacity: 0.5; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeLaunchLabel; Layout.preferredWidth: 80; horizontalAlignment: Text.AlignRight }
                    Text { text: "Starting..."; color: Theme.color1; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeLaunchLabel; Layout.fillWidth: true }

                    Text { text: "Login:"; color: Theme.color1; opacity: 0.5; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeLaunchLabel; horizontalAlignment: Text.AlignRight }
                    Text { text: "Offline"; color: Theme.color1; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeLaunchLabel }

                    Text { text: "Progress:"; color: Theme.color1; opacity: 0.5; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeLaunchLabel; horizontalAlignment: Text.AlignRight }
                    Text { text: "0%"; color: Theme.color1; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeLaunchLabel }
                }
            }

            Item { Layout.fillHeight: true }

            // Status label
            Text {
                id: statusLabel
                Layout.alignment: Qt.AlignHCenter
                Layout.bottomMargin: 10
                text: "Ready"
                color: Theme.gray3
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeSmall
            }

            // Cancel button
            MyButton {
                Layout.fillWidth: true
                Layout.preferredHeight: Theme.versionBtnHeight
                Layout.leftMargin: 20
                Layout.rightMargin: 20
                Layout.bottomMargin: 20
                text: "Cancel"
                colorType: 2 // Red
                onClicked: {
                    Launcher.interrupt()
                    panLaunching.opacity = 0
                    panLaunching.visible = false
                    panInput.opacity = 1
                    statusLabel.text = "Cancelled"
                }
            }
        }
    }

    // State helpers
    property int loginType: 0 // 0=Legacy, 5=Ms

    function loginTypeText() {
        return loginType === 5 ? "Microsoft Login" : "Offline Login"
    }

    function pageOnEnter() {
        panInput.opacity = 1
        panLaunching.visible = false
        panLaunching.opacity = 0
    }

    // Connections
    Connections {
        target: Launcher
        function onStateChanged() {
            if (Launcher.state === Launcher.Running) {
                launchTitle.text = "Running"
            } else if (Launcher.state === Launcher.Finished) {
                panLaunching.opacity = 0
                panLaunching.visible = false
                panInput.opacity = 1
                statusLabel.text = "Game exited"
            } else if (Launcher.state === Launcher.Failed) {
                panLaunching.opacity = 0
                panLaunching.visible = false
                panInput.opacity = 1
                statusLabel.text = "Launch failed"
            }
        }
    }
}
