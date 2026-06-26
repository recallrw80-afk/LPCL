import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PCL.Core
import "src/ui/components"
import "src/ui/pages"
import "src/ui/styles"

ApplicationWindow {
    id: window
    width: 960
    height: 640
    minimumWidth: 640
    minimumHeight: 480
    visible: true
    title: "PCL_LIUNX - Minecraft Launcher"

    // Dark theme (matching original PCL)
    color: Theme.bgPrimary

    // Status bar
    footer: ToolBar {
        background: Rectangle {
            color: Theme.bgSecondary
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.spacingMd
            anchors.rightMargin: Theme.spacingMd
            spacing: Theme.spacingMd

            Label {
                text: Launcher.statusText || qsTr("Ready")
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeXs
                Layout.fillWidth: true
                elide: Text.ElideRight
            }

            Label {
                text: qsTr("Java: ") + JavaManager.selectedJavaName
                color: Theme.textSecondary
                font.pixelSize: Theme.fontSizeXs
            }

            MyProgressBar {
                Layout.preferredWidth: 120
                Layout.preferredHeight: 4
                value: Launcher.isRunning ? 1.0 : 0.0
                visible: Launcher.state !== Launcher.Idle && Launcher.state !== Launcher.Finished
            }
        }
    }

    // Main content
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Left sidebar
        Rectangle {
            Layout.preferredWidth: 56
            Layout.fillHeight: true
            color: Theme.bgSecondary

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spacingXs
                spacing: Theme.spacingXs

                // Tab buttons
                MyButton {
                    id: launchTabBtn
                    text: qsTr("Play")
                    bgColor: tabBar.currentIndex === 0 ? Theme.accent : "transparent"
                    bgHover: tabBar.currentIndex === 0 ? Theme.accentHover : Theme.bgInput
                    textColor: Theme.textPrimary
                    radius: Theme.radiusMd
                    Layout.fillWidth: true
                    implicitHeight: 42
                    font.pixelSize: Theme.fontSizeSm
                    onClicked: tabBar.currentIndex = 0
                }

                MyButton {
                    text: qsTr("Login")
                    bgColor: tabBar.currentIndex === 1 ? Theme.accent : "transparent"
                    bgHover: tabBar.currentIndex === 1 ? Theme.accentHover : Theme.bgInput
                    textColor: Theme.textPrimary
                    radius: Theme.radiusMd
                    Layout.fillWidth: true
                    implicitHeight: 42
                    font.pixelSize: Theme.fontSizeSm
                    onClicked: tabBar.currentIndex = 1
                }

                MyButton {
                    text: qsTr("Settings")
                    bgColor: tabBar.currentIndex === 2 ? Theme.accent : "transparent"
                    bgHover: tabBar.currentIndex === 2 ? Theme.accentHover : Theme.bgInput
                    textColor: Theme.textPrimary
                    radius: Theme.radiusMd
                    Layout.fillWidth: true
                    implicitHeight: 42
                    font.pixelSize: Theme.fontSizeSm
                    onClicked: tabBar.currentIndex = 2
                }

                Item { Layout.fillHeight: true }
            }
        }

        // Right content area
        StackLayout {
            id: tabBar
            Layout.fillWidth: true
            Layout.fillHeight: true

            LaunchPage {}
            LoginPage {}
            SettingsPage {}
        }
    }

    // Initialize on startup
    Component.onCompleted: {
        // Java scan is already started in main.cpp
        // Load versions — mcFolder is set in main.cpp from saved settings
        VersionManager.loadLocalVersions()
    }
}
