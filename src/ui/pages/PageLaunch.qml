import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import LPCL.Core
import "../components"
import "../styles"

// Launch tab — full replica of Windows PCL launch experience
// PanInput (default) ←→ PanLaunching (during launch) ←→ PanRunning (game live)
Item {
    id: page
    property bool isActive: false
    property int loginType: 5              // 0=Legacy, 5=Ms, 2=Nide, 3=Auth
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

    // Auto-select first version on load
    function refreshVersions() {
        VersionManager.loadLocalVersions()
        VersionManager.fetchVersionManifest()
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        
        // Left sidebar
        
        Rectangle {
            Layout.preferredWidth: 280
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

            Flickable {
                anchors.fill: parent
                contentWidth: width
                contentHeight: panLeftContent.implicitHeight + 30
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: LPCLScrollBar {}

                ColumnLayout {
                    id: panLeftContent
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    spacing: 0

                    // ---- PanInput: login + version + launch ----
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: panInputContent.implicitHeight + 20
                        visible: !isLaunching && !isRunning

                        ColumnLayout {
                            id: panInputContent
                            anchors { left: parent.left; right: parent.right; top: parent.top }
                            spacing: 0

                            // Login type buttons
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

                            // Account display / name input
                            Item {
                                Layout.fillWidth: true; Layout.preferredHeight: 60

                                // Logged in (either mode): show account name
                                Text {
                                    anchors.centerIn: parent
                                    visible: accountName !== ""
                                    text: accountName
                                    color: Theme.color3; font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeTitle
                                    horizontalAlignment: Text.AlignHCenter
                                    width: parent.width - 20
                                }

                                // Offline mode — name input
                                LPCLTextBox {
                                    anchors.centerIn: parent
                                    visible: loginType === 0 && !accountName
                                    width: parent.width - 40
                                    placeholderText: "输入角色名"
                                    text: "Player"
                                    onTextChanged: accountName = text
                                }

                                // MS mode — click to start login
                                Text {
                                    anchors.centerIn: parent
                                    visible: loginType === 5 && !accountName && !msPolling
                                    text: "点击登录"
                                    color: Theme.color2; font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeTitle
                                    horizontalAlignment: Text.AlignHCenter
                                }
                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    enabled: loginType === 5 && !accountName && !msPolling
                                    onClicked: startMsLogin()
                                }

                                // MS polling — show device code
                                Text {
                                    anchors.centerIn: parent
                                    visible: msPolling
                                    text: "请在浏览器中输入: " + msDeviceCode
                                    color: Theme.color3; font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXSmall
                                    horizontalAlignment: Text.AlignHCenter; wrapMode: Text.WordWrap
                                    width: parent.width - 20
                                }
                            }

                            // MS auth URL hint
                            Item {
                                Layout.fillWidth: true; Layout.preferredHeight: 22
                                visible: msPolling && msVerifyUrl !== ""
                                Text {
                                    anchors.centerIn: parent
                                    text: msVerifyUrl
                                    color: Theme.color2; font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXSmall
                                    MouseArea {
                                        anchors.fill: parent
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: Qt.openUrlExternally(msVerifyUrl)
                                    }
                                }
                            }

                            // Version selector pill
                            Rectangle {
                                Layout.alignment: Qt.AlignHCenter
                                Layout.topMargin: 24; Layout.bottomMargin: 4
                                width: Math.min(verPillRow.implicitWidth + 24, 240)
                                height: 27; radius: 13; color: Theme.semiTransparent

                                Row {
                                    id: verPillRow; anchors.centerIn: parent; spacing: 8
                                    LPCLIcon { size: 16; anchors.verticalCenter: parent.verticalCenter; lucideIcon: "layout-grid"; iconColor: Theme.color3 }
                                    Text {
                                        text: selectedVersion || "选择版本"
                                        font.family: Theme.fontFamily; font.pixelSize: 14; color: Theme.color3
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                                MouseArea {
                                    anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                    onClicked: { versionListPopup.visible = !versionListPopup.visible }
                                }
                            }

                            // Version list popup
                            Rectangle {
                                id: versionListPopup
                                Layout.alignment: Qt.AlignHCenter
                                Layout.preferredWidth: 200; Layout.preferredHeight: Math.min(300, verListView.contentHeight + 10)
                                visible: false; radius: 6
                                color: Theme.pureWhite
                                border { width: 1; color: Theme.gray5 }
                                z: 10

                                Flickable {
                                    anchors { fill: parent; margins: 5 }
                                    contentWidth: width; contentHeight: verListView.contentHeight
                                    clip: true
                                    ScrollBar.vertical: LPCLScrollBar {}

                                    ColumnLayout {
                                        id: verListView
                                        anchors { left: parent.left; right: parent.right }
                                        spacing: 2
                                        Repeater {
                                            model: VersionManager.versionIds
                                            Rectangle {
                                                Layout.fillWidth: true; Layout.preferredHeight: 26; radius: 3
                                                color: verMouse.containsMouse ? Theme.color7 : "transparent"
                                                Text {
                                                    anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
                                                    text: modelData; color: Theme.color1
                                                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize
                                                    elide: Text.ElideRight
                                                }
                                                MouseArea {
                                                    id: verMouse; anchors.fill: parent; hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: {
                                                        selectedVersion = modelData
                                                        versionListPopup.visible = false
                                                        statusMessage = "已选择 " + selectedVersion + "，点击启动"
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            Item { Layout.fillHeight: true; Layout.preferredHeight: 15 }

                            // Launch button
                            Item {
                                Layout.fillWidth: true; Layout.preferredHeight: 70
                                LPCLButton {
                                    anchors { horizontalCenter: parent.horizontalCenter; top: parent.top }
                                    width: parent.width - 40; height: Theme.launchBtnHeight
                                    text: "启动游戏"; colorType: 1
                                    enabled: selectedVersion !== "" && !isLaunching && !isRunning
                                    onClicked: doLaunch()
                                }
                                Text {
                                    anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom }
                                    text: statusMessage; font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeXSmall; color: Theme.gray3
                                }
                            }
                            Item { Layout.preferredHeight: 20 }
                        }
                    }

                    // ---- PanLaunching: progress overlay ----
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: panLaunchInner.implicitHeight + 40
                        visible: isLaunching || isRunning

                        ColumnLayout {
                            id: panLaunchInner
                            anchors { left: parent.left; right: parent.right; top: parent.top }
                            anchors.margins: 20; spacing: 0

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
                                text: selectedVersion; font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLaunchName; color: Theme.color3
                                Layout.alignment: Qt.AlignHCenter; Layout.topMargin: 5; Layout.bottomMargin: 12
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

                            GridLayout {
                                Layout.alignment: Qt.AlignHCenter
                                columns: 2; rowSpacing: 5; columnSpacing: 15
                                Text { text: "当前步骤"; font: smallFont; color: Theme.gray3; opacity: 0.5; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 80 }
                                Text { text: Launcher.statusText; font: smallFont; color: Theme.color3; Layout.preferredWidth: 100 }
                                Text { text: "登录方式"; font: smallFont; color: Theme.gray3; opacity: 0.5; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 80 }
                                Text { text: loginType === 0 ? "离线登录" : (loginType === 5 ? "正版登录" : "第三方登录"); font: smallFont; color: Theme.color3; Layout.preferredWidth: 100 }
                                Text { text: "启动进度"; font: smallFont; color: Theme.gray3; opacity: 0.5; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 80 }
                                Text { text: Launcher.progress.toFixed(1) + " %"; font: smallFont; color: Theme.color3; Layout.preferredWidth: 100 }
                            }

                            LPCLButton {
                                Layout.alignment: Qt.AlignHCenter; Layout.topMargin: 20
                                text: isRunning ? "结束游戏" : "取消启动"
                                colorType: 2; Layout.preferredWidth: 120
                                onClicked: Launcher.interrupt()
                            }
                        }
                    }

                }
            }
        }

        
        // Right content — custom home cards + launch log
        
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

                    // ---- Custom home cards (replica of Windows Custom.xaml) ----
                    // Card: 快速启动
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: quickCardCol.height + 36
                        radius: Theme.buttonRadius; color: Theme.pureWhite
                        border { width: 1; color: Theme.gray5 }
                        visible: selectedVersion !== ""

                        ColumnLayout {
                            id: quickCardCol
                            anchors { left: parent.left; right: parent.right; top: parent.top }
                            anchors.margins: 20; spacing: 12

                            Text { text: "快速启动"; font: boldFont; color: Theme.color1 }

                            RowLayout {
                                spacing: 10
                                LPCLButton {
                                    text: "启动游戏"; colorType: 1; Layout.preferredWidth: 120
                                    enabled: selectedVersion !== ""
                                    onClicked: doLaunch()
                                }
                                LPCLButton {
                                    text: "版本设置"; colorType: 0; Layout.preferredWidth: 120
                                    onClicked: navTabs.currentIndex = 2  // jump to settings
                                }
                            }

                            Text {
                                text: "版本: " + (selectedVersion || "未选择") + "\n目录: " + VersionManager.mcFolder
                                font: smallFont; color: Theme.gray3; wrapMode: Text.WordWrap; Layout.fillWidth: true
                            }
                        }
                    }

                    // Card: 启动日志
                    Rectangle {
                        Layout.fillWidth: true; Layout.fillHeight: true; Layout.minimumHeight: 150
                        radius: Theme.buttonRadius; color: Theme.pureWhite
                        border { width: 1; color: Theme.gray5 }

                        ColumnLayout {
                            anchors.fill: parent; spacing: 0
                            Rectangle {
                                Layout.fillWidth: true; Layout.preferredHeight: 38; color: "transparent"
                                RowLayout {
                                    anchors { left: parent.left; right: parent.right; top: parent.top }
                                    anchors.margins: 20; anchors.topMargin: 18
                                    Text { text: "启动日志"; font: boldFont; color: Theme.color1 }
                                    Item { Layout.fillWidth: true }
                                    LPCLButton {
                                        text: "清空"; Layout.preferredWidth: 50; Layout.preferredHeight: 22
                                        colorType: 0; padding: 4
                                        onClicked: labLog.text = "LPCL v" + Qt.application.version + "\nReady.\n"
                                    }
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

            // Game log connection
            Connections {
                target: Launcher
                function onGameLog(line) { labLog.text += line + "\n" }
            }
        }
    }

    ====
    // Fonts
    ====
    readonly property font smallFont: Qt.font({ family: Theme.fontFamily, pixelSize: Theme.fontSizeLaunchLabel })
    readonly property font boldFont: Qt.font({ family: Theme.fontFamily, pixelSize: Theme.fontSizeLaunchName, bold: true })

    ====
    // State bindings
    ====
    property string statusMessage: "正在加载版本列表，请稍候"

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

    ====
    // Version list
    ====
    Connections {
        target: VersionManager
        function onVersionListChanged() {
            if (VersionManager.versionIds.length > 0 && !selectedVersion) {
                selectedVersion = VersionManager.versionIds[0]
                statusMessage = "已选择 " + selectedVersion + "，点击启动"
            }
            if (VersionManager.versionIds.length > 0) {
                statusMessage = statusMessage || ("已找到 " + VersionManager.versionCount + " 个版本")
            }
        }
    }

    ====
    // Launch
    ====
    function doLaunch() {
        if (!selectedVersion) return
        // For offline login, generate credentials
        if (loginType === 0) {
            accountName = "Player"
            accountUuid = OfflineAuth.generateOfflineUuid("Player")
        }
        // For MS login without account, fall back to offline
        if (loginType === 5 && !accountName) {
            accountName = "Player"
            accountUuid = OfflineAuth.generateOfflineUuid(accountName)
        }
        labLog.text += "Launching " + selectedVersion + " as " + accountName + "\n"
        Launcher.launchVersion(selectedVersion)
    }

    ====
    // Microsoft OAuth login (MsAuth instance pre-created for signal wiring)
    ====
    MsAuth {
        id: msAuth
        onDeviceCodeReady: function(code, url) {
            msDeviceCode = code
            msVerifyUrl = url
            statusMessage = "请在浏览器中输入验证码"
        }
        onLoginProgress: function(status) {
            statusMessage = status
        }
        onLoginFinished: function(success, result) {
            msPolling = false
            if (success) {
                accountName = result.name
                accountUuid = result.uuid
                statusMessage = "已登录: " + accountName
            } else {
                statusMessage = "登录失败，将使用离线模式"
                loginType = 0
            }
        }
    }

    function startMsLogin() {
        msPolling = true
        msAuth.startLogin()
    }

    ====
    // Page lifecycle
    ====
    function pageOnEnter() {
        refreshVersions()
    }
}
