import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import LPCL.Core
import "../components"
import "../styles"

// Exact replica of PageLaunchLeft.xaml layout
// PanInput (default) ←→ PanLaunching (during launch)
Item {
    id: page
    property bool isActive: false
    property int loginType: 0              // 0=Offline, 5=Ms
    property string selectedVersion: ""
    property string accountName: ""
    property string accountUuid: ""
    property string msDeviceCode: ""
    property string msVerifyUrl: ""
    property bool msPolling: false

    visible: opacity > 0
    opacity: isActive ? 1 : 0
    scale: isActive ? 1 : 0.96
    Behavior on opacity { NumberAnimation { duration: 100 } }
    Behavior on scale { NumberAnimation { duration: 400; easing.type: Easing.OutBack } }

    function refreshVersions() {
        VersionManager.loadLocalVersions()
        VersionManager.fetchVersionManifest()
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ================================================================
        // Left sidebar — Width=300, matches original MyPageLeft
        // ================================================================
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

            // ---- PanInput ----
            Item {
                id: panInput
                anchors.fill: parent
                visible: !isLaunching && !isRunning

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Login type buttons (正版 / 离线)
                    RowLayout {
                        Layout.preferredHeight: 35
                        Layout.topMargin: 22
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 8

                        LPCLButton {
                            id: msBtn
                            padding: 15; radius: height / 2
                            colorType: loginType === 5 ? 1 : 0
                            contentItem: Row {
                                spacing: 6
                                LPCLIcon { size: 16; lucideIcon: "shield-check"; anchors.verticalCenter: parent.verticalCenter; iconColor: msBtn.labelColor }
                                Text { text: "正版"; color: msBtn.labelColor; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize; anchors.verticalCenter: parent.verticalCenter }
                            }
                            onClicked: { loginType = 5 }
                        }
                        LPCLButton {
                            id: offlineBtn
                            padding: 15; radius: height / 2
                            colorType: loginType === 0 ? 1 : 0
                            contentItem: Row {
                                spacing: 6
                                LPCLIcon { size: 16; lucideIcon: "unlink"; anchors.verticalCenter: parent.verticalCenter; iconColor: offlineBtn.labelColor }
                                Text { text: "离线"; color: offlineBtn.labelColor; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize; anchors.verticalCenter: parent.verticalCenter }
                            }
                            onClicked: {
                                loginType = 0
                                accountName = "Player"
                                accountUuid = OfflineAuth.generateOfflineUuid("Player")
                            }
                        }
                    }

                    // Row 0: Avatar + username (PanLogin area)
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 70
                        Layout.topMargin: 10

                        // Avatar placeholder circle
                        Rectangle {
                            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top }
                            width: 40; height: 40; radius: 20
                            color: Theme.color7
                            border { width: 1; color: Theme.gray5 }

                            Text {
                                anchors.centerIn: parent
                                text: accountName ? accountName.charAt(0).toUpperCase() : "?"
                                font.family: Theme.fontFamily; font.pixelSize: 18; font.bold: true
                                color: Theme.color2
                            }
                        }

                        // Username below avatar
                        Text {
                            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top; topMargin: 44 }
                            text: accountName || (loginType === 0 ? "输入角色名" : "未登录")
                            font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSmall
                            color: Theme.gray3
                        }
                    }

                    // Spacer: fills remaining space
                    Item { Layout.fillHeight: true; Layout.minimumHeight: 10 }

                    // Launch button + version status
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 75

                        // BtnLaunch — Height=54, Margin=20,0, Padding=30,0,30,15, ColorType=Highlight
                        LPCLButton {
                            id: btnLaunch
                            anchors { left: parent.left; right: parent.right; top: parent.top }
                            anchors.margins: 20
                            height: 54
                            colorType: 1
                            text: "启动游戏"
                            enabled: selectedVersion !== ""
                            padding: 0
                            onClicked: doLaunch()
                        }

                        // LabVersion — FontSize=11, ColorBrushGray3, below button
                        Text {
                            anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom }
                            anchors.bottomMargin: 2
                            text: statusText
                            font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeXSmall
                            color: Theme.gray3; horizontalAlignment: Text.AlignHCenter
                            width: parent.width - 40; elide: Text.ElideRight
                        }
                    }

                    // Row 3: Version select button + version settings
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 45
                        Layout.topMargin: 0
                        Layout.leftMargin: 20
                        Layout.rightMargin: 10

                        LPCLButton {
                            id: btnVersion
                            Layout.fillWidth: true
                            Layout.preferredHeight: 35
                            colorType: 0
                            text: selectedVersion || "版本选择"
                            onClicked: versionPopup.visible = !versionPopup.visible
                        }
                    }

                    // Row 4: Bottom spacer (20px in original)
                    Item { Layout.preferredHeight: 20 }
                }
            }

            // ---- Version selection popup ----
            Rectangle {
                id: versionPopup
                anchors {
                    left: parent.left; right: parent.right; bottom: parent.bottom
                    leftMargin: 20; rightMargin: 10; bottomMargin: 20
                }
                height: Math.min(250, verListContent.contentHeight + 10)
                visible: false; radius: 6; z: 10
                color: Theme.pureWhite
                border { width: 1; color: Theme.gray5 }

                Flickable {
                    id: verListContent
                    anchors { fill: parent; margins: 5 }
                    contentWidth: width; contentHeight: verCol.implicitHeight
                    clip: true
                    ScrollBar.vertical: LPCLScrollBar {}

                    ColumnLayout {
                        id: verCol
                        anchors { left: parent.left; right: parent.right }
                        spacing: 1
                        Repeater {
                            model: VersionManager.versionIds
                            Rectangle {
                                Layout.fillWidth: true; Layout.preferredHeight: 28; radius: 3
                                color: vm.containsMouse ? Theme.color7 : "transparent"
                                Text {
                                    anchors { left: parent.left; leftMargin: 10; verticalCenter: parent.verticalCenter }
                                    text: modelData; color: Theme.color1
                                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize
                                }
                                MouseArea {
                                    id: vm; anchors.fill: parent; hoverEnabled: true
                                    onClicked: {
                                        selectedVersion = modelData
                                        versionPopup.visible = false
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ---- PanLaunching: launch overlay ----
            Item {
                id: panLaunching
                anchors.fill: parent
                visible: isLaunching || isRunning
                opacity: visible ? 1 : 0
                scale: visible ? 1 : 0.8
                Behavior on opacity { NumberAnimation { duration: 250 } }
                Behavior on scale { NumberAnimation { duration: 400; easing.type: Easing.OutBack } }

                ColumnLayout {
                    anchors.centerIn: parent
                    width: parent.width - 40
                    spacing: 0

                    LPCLProgressBar {
                        Layout.fillWidth: true; Layout.preferredHeight: 50
                        indeterminate: Launcher.launchState < Launcher.Downloading
                    }

                    Text {
                        text: launchTitleText
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeLaunchTitle
                        color: Theme.color3; Layout.alignment: Qt.AlignHCenter; Layout.topMargin: 10
                    }
                    Text {
                        text: selectedVersion
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeLaunchName
                        color: Theme.color3; Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 5; Layout.bottomMargin: 12
                    }

                    // Progress bar
                    Rectangle {
                        Layout.fillWidth: true; Layout.preferredHeight: 4
                        Layout.topMargin: 12; Layout.bottomMargin: 27; radius: 2
                        color: Theme.gray6; opacity: 0.6
                        Rectangle {
                            width: parent.width * (Launcher.progress / 100.0)
                            height: parent.height; radius: 2
                            gradient: Gradient {
                                GradientStop { position: 0; color: Theme.color4 }
                                GradientStop { position: 0.6; color: Theme.color3 }
                            }
                        }
                    }

                    // Status info
                    GridLayout {
                        Layout.alignment: Qt.AlignHCenter
                        columns: 2; rowSpacing: 5; columnSpacing: 15
                        Text { text: "当前步骤"; font: infoFont; color: Theme.gray3; opacity: 0.5; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 80 }
                        Text { text: Launcher.statusText; font: infoFont; color: Theme.color3; Layout.preferredWidth: 100 }
                        Text { text: "启动进度"; font: infoFont; color: Theme.gray3; opacity: 0.5; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 80 }
                        Text { text: Launcher.progress.toFixed(1) + " %"; font: infoFont; color: Theme.color3; Layout.preferredWidth: 100 }
                    }

                    LPCLButton {
                        Layout.alignment: Qt.AlignHCenter; Layout.topMargin: 20
                        text: isRunning ? "结束游戏" : "取消"
                        colorType: 2; Layout.preferredWidth: 120
                        onClicked: Launcher.interrupt()
                    }
                }
            }
        }

        // ================================================================
        // Right content — version info + launch log
        // ================================================================
        Rectangle {
            Layout.fillWidth: true; Layout.fillHeight: true; color: "transparent"

            Flickable {
                anchors.fill: parent; contentWidth: width
                contentHeight: panMain.implicitHeight + 25; clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: LPCLScrollBar {}

                ColumnLayout {
                    id: panMain
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    anchors.margins: 25; spacing: 15

                    // Quick launch card
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: quickCardCol.height + 36
                        radius: Theme.buttonRadius; color: Theme.pureWhite
                        border { width: 1; color: Theme.gray5 }

                        ColumnLayout {
                            id: quickCardCol
                            anchors { left: parent.left; right: parent.right; top: parent.top }
                            anchors.margins: 20; spacing: 12

                            Text { text: "快速启动"; font: boldFont; color: Theme.color1 }
                            Text {
                                text: "版本: " + (selectedVersion || "未选择") + "\n目录: " + VersionManager.mcFolder
                                font: smallFont; color: Theme.gray3; Layout.fillWidth: true; wrapMode: Text.WordWrap
                            }
                        }
                    }

                    // Log card
                    Rectangle {
                        Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 150
                        radius: Theme.buttonRadius; color: Theme.pureWhite
                        border { width: 1; color: Theme.gray5 }

                        ColumnLayout {
                            anchors.fill: parent; spacing: 0
                            Rectangle {
                                Layout.fillWidth: true; Layout.preferredHeight: 38; color: "transparent"
                                Text {
                                    anchors { left: parent.left; top: parent.top; leftMargin: 20; topMargin: 18 }
                                    text: "启动日志"; font: boldFont; color: Theme.color1
                                }
                            }
                            Flickable {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                Layout.leftMargin: 20; Layout.rightMargin: 23; Layout.bottomMargin: 18
                                clip: true; contentWidth: width; contentHeight: labLog.implicitHeight
                                ScrollBar.vertical: LPCLScrollBar {}
                                Text {
                                    id: labLog; width: parent.width
                                    text: "LPCL v" + Qt.application.version + "\nReady.\n"
                                    color: Theme.color1; font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSize; wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }

            Connections {
                target: Launcher
                function onGameLog(line) { labLog.text += line + "\n" }
            }
        }
    }

    // ====================================================================
    // Fonts
    // ====================================================================
    readonly property font smallFont: Qt.font({ family: Theme.fontFamily, pixelSize: Theme.fontSizeSmall })
    readonly property font infoFont: Qt.font({ family: Theme.fontFamily, pixelSize: Theme.fontSizeLaunchLabel })
    readonly property font boldFont: Qt.font({ family: Theme.fontFamily, pixelSize: Theme.fontSizeLaunchName, bold: true })

    // ====================================================================
    // State
    // ====================================================================
    property string statusText: "正在加载版本列表，请稍候"

    readonly property bool isLaunching: {
        var s = Launcher.launchState
        return s >= Launcher.Prechecking && s < Launcher.Running
    }
    readonly property bool isRunning: Launcher.launchState === Launcher.Running

    readonly property string launchTitleText: {
        if (isRunning) return "游戏运行中"
        if (Launcher.launchState === Launcher.Failed) return "启动失败"
        if (Launcher.launchState === Launcher.Finished) return "游戏已退出"
        return "正在启动游戏"
    }

    // ---- Version list loaded ----
    Connections {
        target: VersionManager
        function onVersionListChanged() {
            if (VersionManager.versionIds.length > 0 && !selectedVersion) {
                selectedVersion = VersionManager.versionIds[0]
                statusText = "已选择 " + selectedVersion + "，点击启动"
            }
        }
    }

    // ---- MS OAuth ----
    MsAuth {
        id: msAuth
        onDeviceCodeReady: function(code, url) {
            msDeviceCode = code
            msVerifyUrl = url
        }
        onLoginProgress: function(status) { statusText = status }
        onLoginFinished: function(success, result) {
            msPolling = false
            if (success) {
                accountName = result.name
                accountUuid = result.uuid
                statusText = "已登录: " + accountName
            } else {
                loginType = 0
            }
        }
    }

    // ---- Launch ----
    function doLaunch() {
        if (!selectedVersion) return
        if (!accountName) {
            accountName = "Player"
            accountUuid = OfflineAuth.generateOfflineUuid(accountName)
        }
        Launcher.launchVersion(selectedVersion)
    }

    function startMsLogin() {
        msPolling = true
        msAuth.startLogin()
    }

    function pageOnEnter() {
        refreshVersions()
    }
}
