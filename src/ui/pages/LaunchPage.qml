import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PCL.Core
import "../components"
import "../styles"

// Launch page - main page for version selection and game launching
Page {
    id: page

    // Local state
    property string selectedVersionId: ""
    property string selectedJavaPath: ""
    property string playerName: "Player"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.spacingMd
        spacing: Theme.spacingMd

        // Header
        Text {
            text: qsTr("Minecraft Launcher")
            font.pixelSize: Theme.fontSizeXl
            font.bold: true
            color: Theme.textPrimary
            Layout.bottomMargin: Theme.spacingSm
        }

        // Version selector
        GroupBox {
            title: qsTr("Version")
            Layout.fillWidth: true

            background: Rectangle {
                radius: Theme.radiusMd
                color: Theme.bgCard
                border.color: Theme.border
            }

            RowLayout {
                anchors.fill: parent
                spacing: Theme.spacingMd

                MyComboBox {
                    id: versionCombo
                    Layout.fillWidth: true
                    model: VersionManager.versionIds
                    enabled: !Launcher.isRunning
                    onCurrentIndexChanged: {
                        if (currentIndex >= 0) {
                            selectedVersionId = currentText
                        }
                    }
                }

                MyButton {
                    text: qsTr("Refresh")
                    bgColor: Theme.bgInput
                    bgHover: Theme.border
                    textColor: Theme.textPrimary
                    enabled: !Launcher.isRunning && !VersionManager.isLoading
                    onClicked: VersionManager.loadLocalVersions()
                }
            }
        }

        // Login section
        GroupBox {
            title: qsTr("Login")
            Layout.fillWidth: true

            background: Rectangle {
                radius: Theme.radiusMd
                color: Theme.bgCard
                border.color: Theme.border
            }

            RowLayout {
                anchors.fill: parent
                spacing: Theme.spacingMd

                Label {
                    text: qsTr("Username:")
                    color: Theme.textSecondary
                    Layout.preferredWidth: 80
                }

                MyTextField {
                    id: usernameField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Enter player name")
                    text: playerName
                    enabled: !Launcher.isRunning
                    onTextChanged: playerName = text
                }

                MyButton {
                    id: loginBtn
                    text: qsTr("Login")
                    bgColor: Theme.bgInput
                    bgHover: Theme.border
                    textColor: Theme.textPrimary
                    enabled: !Launcher.isRunning
                    onClicked: {
                        if (usernameField.text.trim() !== "") {
                            statusText.text = qsTr("Logged in as: ") + usernameField.text.trim()
                        }
                    }
                }
            }
        }

        // Launch options
        GroupBox {
            title: qsTr("Options")
            Layout.fillWidth: true

            background: Rectangle {
                radius: Theme.radiusMd
                color: Theme.bgCard
                border.color: Theme.border
            }

            GridLayout {
                anchors.fill: parent
                columns: 2
                rowSpacing: Theme.spacingSm
                columnSpacing: Theme.spacingMd

                Label { text: qsTr("Max Memory (MB):"); color: Theme.textSecondary }
                MyTextField {
                    id: maxMemoryField
                    text: "4096"
                    Layout.fillWidth: true
                    enabled: !Launcher.isRunning
                }

                Label { text: qsTr("Java:"); color: Theme.textSecondary }
                MyComboBox {
                    id: javaCombo
                    Layout.fillWidth: true
                    model: JavaManager.javaNames
                    enabled: !Launcher.isRunning
                }
            }
        }

        // Launch button
        MyButton {
            id: launchBtn
            text: Launcher.isRunning ? qsTr("Running...") : qsTr("Launch Minecraft")
            font.pixelSize: Theme.fontSizeLg
            Layout.fillWidth: true
            implicitHeight: 48
            enabled: !Launcher.isRunning && versionCombo.currentIndex >= 0 && JavaManager.javaCount > 0
            bgColor: Launcher.isRunning ? Theme.textDisabled : Theme.accent
            bgHover: Launcher.isRunning ? Theme.textDisabled : Theme.accentHover

            onClicked: {
                if (Launcher.isRunning) return
                if (JavaManager.javaCount === 0) {
                    statusText.text = qsTr("No Java found. Check Settings > Java > Scan for Java.")
                    return
                }
                statusText.text = qsTr("Launch requires full integration (C++ side). Ready for Phase 7.")
                // Full launch will be wired in Phase 7
            }
        }

        // Status
        Label {
            id: statusText
            text: qsTr("Ready — ") + JavaManager.javaCount + qsTr(" Java(s), ") +
                  VersionManager.versionCount + qsTr(" version(s) found.")
            color: Theme.textSecondary
            font.pixelSize: Theme.fontSizeSm
            Layout.fillWidth: true
            wrapMode: Text.Wrap
        }

        // Progress bar
        MyProgressBar {
            id: progressBar
            Layout.fillWidth: true
            visible: VersionManager.isLoading || JavaManager.isScanning
            indeterminate: true
        }

        // Log output
        GroupBox {
            title: qsTr("Log")
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 150

            background: Rectangle {
                radius: Theme.radiusMd
                color: Theme.bgInput
                border.color: Theme.border
            }

            ScrollView {
                anchors.fill: parent
                clip: true

                TextArea {
                    id: logArea
                    readOnly: true
                    color: Theme.textPrimary
                    font.pixelSize: Theme.fontSizeXs
                    font.family: "monospace"
                    background: null
                    text: "PCL_LIUNX v0.1\nReady.\n"
                }
            }
        }
    }

    // Connections
    Connections {
        target: Launcher
        function onGameLog(line) { logArea.append(line) }
        function onStateChanged() {
            switch(Launcher.state) {
                case Launcher.Failed:
                    statusText.text = qsTr("Launch failed");
                    break
                case Launcher.Finished:
                    statusText.text = qsTr("Game exited");
                    break
                case Launcher.Running:
                    statusText.text = qsTr("Game running...");
                    break
            }
        }
    }

    Connections {
        target: JavaManager
        function onScanningChanged() {
            if (!JavaManager.isScanning) {
                statusText.text = qsTr("Ready — ") + JavaManager.javaCount +
                                  qsTr(" Java(s), ") + VersionManager.versionCount +
                                  qsTr(" version(s).")
            }
        }
        function onJavaListChanged() {
            if (!JavaManager.isScanning) {
                statusText.text = qsTr("Ready — ") + JavaManager.javaCount +
                                  qsTr(" Java(s), ") + VersionManager.versionCount +
                                  qsTr(" version(s).")
            }
        }
    }

    Connections {
        target: VersionManager
        function onLoadingChanged() {
            if (!VersionManager.isLoading) {
                statusText.text = qsTr("Ready — ") + JavaManager.javaCount +
                                  qsTr(" Java(s), ") + VersionManager.versionCount +
                                  qsTr(" version(s).")
            }
        }
    }
}
