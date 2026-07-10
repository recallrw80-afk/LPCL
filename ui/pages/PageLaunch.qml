import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import LPCL.Core
import "../components"
import "../styles"

// Exact replica of PageLaunchLeft.xaml + PageLaunchRight.xaml layout + interaction logic
// Single file: left sidebar (300px) + right content
// Two modes: PanInput (default) ↔ PanLaunching (during launch)
// Sub-page: version selection (replaces both panels)
// 4-state launch button matching RefreshButtonsUI()
// 100ms polling timer with smooth progress
Item {
    id: page
    property bool isActive: false
    property int loginType: 0              // 0=Offline, 5=Ms
    property string selectedVersion: ""
    property string accountName: ""
    property string accountUuid: ""
    property var loginResult: null         // Last successful LoginResult (gadget)
    property string msDeviceCode: ""
    property string msVerifyUrl: ""
    property bool msPolling: false

    // ---- External drag-and-drop state (received via C++ FileDropHandler) ----
    property bool dragHovering: false
    property string dragDropResult: ""

    // ---- Identify dropped file type (only active when PageLaunch is visible) ----
    function identifyFileType(filePath) {
        var normalized = filePath.replace(/[\\/]/g, '/');
        var lower = normalized.toLowerCase();
        var leaf = normalized.split('/').pop();

        // Directory (trailing slash or no file extension)
        if (lower.endsWith('/') || (leaf.indexOf('.') === -1 && leaf.length > 0)) {
            return {
                kind: "folder",
                label: "文件夹",
                icon: "folder",
                detail: leaf
            };
        }

        var ext = leaf.split('.').pop().toLowerCase();

        if (ext === "jar") {
            return {
                kind: "jar",
                label: "Mod / Jar 文件",
                icon: "file-archive",
                detail: leaf
            };
        }
        if (ext === "zip") {
            return {
                kind: "zip",
                label: "压缩包",
                icon: "file-archive",
                detail: leaf
            };
        }
        if (ext === "mrpack") {
            return {
                kind: "mrpack",
                label: "Modrinth 整合包",
                icon: "package",
                detail: leaf
            };
        }
        if (ext === "json") {
            return {
                kind: "json",
                label: "JSON 配置文件",
                icon: "file-code",
                detail: leaf
            };
        }
        if (ext === "png" || ext === "jpg" || ext === "jpeg" || ext === "svg") {
            return {
                kind: "image",
                label: "图片文件",
                icon: "image",
                detail: leaf
            };
        }
        if (ext === "txt" || ext === "log") {
            return {
                kind: "text",
                label: "文本文件",
                icon: "file-text",
                detail: leaf
            };
        }

        return {
            kind: "unknown",
            label: "未知文件 (." + ext + ")",
            icon: "file",
            detail: leaf
        };
    }

    function handleFilesDropped(files) {
        if (!isActive || !files || files.length === 0) {
            dragHovering = false;
            return;
        }

        var results = [];
        for (var i = 0; i < files.length; i++) {
            results.push(identifyFileType(files[i]));
        }

        var lines = [];
        for (var j = 0; j < results.length; j++) {
            var r = results[j];
            lines.push("• [" + r.label + "] " + r.detail);
        }
        dragHovering = false;

        console.log(files);
    }

    visible: opacity > 0
    opacity: isActive ? 1 : 0
    scale: isActive ? 1 : 0.96
    Behavior on opacity {
        NumberAnimation {
            duration: 100
        }
    }
    Behavior on scale {
        NumberAnimation {
            duration: 400
            easing.type: Easing.OutBack
        }
    }

    // 4-state launch button (matching RefreshButtonsUI)
    // State 0: Loading — "正在加载", disabled
    // State 1: No versions (download hidden) — "启动游戏", disabled
    // State 2: No versions (download available) — "下载游戏", enabled
    // State 3: Ready — "启动游戏", enabled
    property int btnLaunchState: 0
    property string statusText: "正在加载版本列表，请稍候"

    readonly property string btnLaunchText: {
        switch (btnLaunchState) {
        case 0:
            return "正在加载";
        case 1:
            return "启动游戏";
        case 2:
            return "下载游戏";
        case 3:
            return "启动游戏";
        default:
            return "启动游戏";
        }
    }

    function refreshButtonsUI() {
        if (VersionManager.isLoading) {
            btnLaunchState = 0;
            statusText = "正在加载中，请稍候";
        } else if (VersionManager.versionIds.length === 0) {
            btnLaunchState = 2;
            statusText = "未找到可用的游戏版本";
        } else if (!selectedVersion) {
            btnLaunchState = 1;
            statusText = "未找到可用的游戏版本";
        } else {
            btnLaunchState = 3;
            statusText = selectedVersion;  // Original shows raw version name
        }
    }

    // Launch state
    readonly property bool isLaunching: {
        var s = Launcher.launchState;
        return s >= Launcher.Prechecking && s < Launcher.Running;
    }
    readonly property bool isRunning: Launcher.launchState === Launcher.Running

    // PanInput ↔ PanLaunching transition animations
    // TO launching: PanInput fades+zooms out, PanLaunching bounces in
    // BACK to input: PanLaunching shrinks+fades, PanInput bounces back
    property real panInputOpacity: 1
    property real panInputScale: 1
    property real panLaunchingOpacity: 0
    property real panLaunchingScale: 0.8

    function pageChangeToLaunching() {
        panLaunchingTitle = "正在启动游戏";
        showProgress = 0;
        // Match original timing: PanInput fades immediately, PanLaunching delayed 100ms
        panInputFadeOut.start();
        panInputZoomOut.start();
        panLaunchingDelay.start();
    }

    function pageChangeToLogin() {
        // Match original: PanLaunching shrinks immediately, PanInput delayed 50ms
        panLaunchingFadeOut.start();
        panLaunchingShrink.start();
        panInputDelay.start();
        refreshButtonsUI();
    }

    // PanInput animations
    NumberAnimation {
        id: panInputFadeOut
        target: page
        property: "panInputOpacity"
        to: 0
        duration: 50
    }
    NumberAnimation {
        id: panInputZoomOut
        target: page
        property: "panInputScale"
        to: 1.2
        duration: 160
    }
    NumberAnimation {
        id: panInputFadeIn
        target: page
        property: "panInputOpacity"
        to: 1
        duration: 250
    }
    NumberAnimation {
        id: panInputScaleIn
        target: page
        property: "panInputScale"
        to: 1
        duration: 300
        easing.type: Easing.OutBack
    }

    // PanLaunching animations
    NumberAnimation {
        id: panLaunchingFadeIn
        target: page
        property: "panLaunchingOpacity"
        to: 1
        duration: 150
    }
    NumberAnimation {
        id: panLaunchingScaleIn
        target: page
        property: "panLaunchingScale"
        to: 1
        duration: 500
        easing.type: Easing.OutBack
    }
    NumberAnimation {
        id: panLaunchingFadeOut
        target: page
        property: "panLaunchingOpacity"
        to: 0
        duration: 150
    }
    NumberAnimation {
        id: panLaunchingShrink
        target: page
        property: "panLaunchingScale"
        to: 0.8
        duration: 150
    }

    // Delay timers matching original animation sequence
    Timer {
        id: panLaunchingDelay
        interval: 100
        repeat: false
        onTriggered: {
            panLaunchingFadeIn.start();
            panLaunchingScaleIn.start();
        }
    }
    Timer {
        id: panInputDelay
        interval: 50
        repeat: false
        onTriggered: {
            panInputFadeIn.start();
            panInputScaleIn.start();
        }
    }

    // Launching refresh (100ms polling, matching LaunchingRefresh)
    property real showProgress: 0
    property string panLaunchingTitle: "正在启动游戏"

    Timer {
        id: launchingRefreshTimer
        interval: 100
        running: page.isLaunching || page.isRunning
        repeat: true
        onTriggered: launchingRefresh()
    }

    function launchingRefresh() {
        if (isRunning) {
            panLaunchingTitle = "已启动游戏";
        } else if (Launcher.launchState === Launcher.Failed) {
            panLaunchingTitle = "启动失败";
        } else if (Launcher.launchState === Launcher.Finished) {
            panLaunchingTitle = "游戏已退出";
        } else {
            panLaunchingTitle = "正在启动游戏";
        }

        var actual = Launcher.progress;
        if (actual < showProgress) {
            showProgress = actual;
        } else if (isRunning) {
            showProgress = 100;
        } else {
            showProgress += (actual - showProgress) * 0.1 + 0.0025;
            if (showProgress > actual)
                showProgress = actual;
        }
    }

    // Sub-page navigation (version selection)
    property bool isSubPage: false
    property string subPageTitle: ""

    function pushSubPage(title) {
        subPageTitle = title;
        isSubPage = true;
    }
    function popSubPage() {
        isSubPage = false;
        subPageTitle = "";
    }

    // Launch state watcher
    Connections {
        target: Launcher
        function onStateChanged() {
            var s = Launcher.launchState;
            if (s >= Launcher.Prechecking && s < Launcher.Running) {
                pageChangeToLaunching();
            } else if (s === Launcher.Finished || s === Launcher.Failed || s === Launcher.Interrupted) {
                pageChangeToLogin();
            }
        }
    }

    // External file drag-and-drop (only acts when this page is active)
    Connections {
        target: FileDropHandler
        function onDragEntered() {
            if (page.isActive)
                page.dragHovering = true;
        }
        function onDragExited() {
            page.dragHovering = false;
        }
        function onFilesDropped(files) {
            page.handleFilesDropped(files);
        }
    }

    // Version list loaded
    Connections {
        target: VersionManager
        function onVersionListChanged() {
            refreshButtonsUI();
            if (VersionManager.versionIds.length > 0 && !selectedVersion) {
                selectedVersion = VersionManager.versionIds[0];
            }
        }
        function onLoadingChanged() {
            refreshButtonsUI();
        }
    }

    // Main layout: left sidebar (300px) + right content
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ---- Left sidebar ----
        Rectangle {
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            color: Theme.sidebarBg

            // Sidebar shadow
            Rectangle {
                anchors {
                    right: parent.right
                    top: parent.top
                    bottom: parent.bottom
                }
                width: 4
                opacity: 0.04
                gradient: Gradient {
                    GradientStop {
                        position: 0
                        color: "#000000"
                    }
                    GradientStop {
                        position: 1
                        color: "#00000000"
                    }
                }
            }

            // ---- Main view (launch sidebar) ----
            Item {
                id: mainView
                anchors.fill: parent
                visible: !isSubPage

                // PanInput — Default mode
                Item {
                    id: panInput
                    anchors.fill: parent
                    visible: panInputOpacity > 0
                    opacity: panInputOpacity
                    scale: panInputScale
                    transformOrigin: Item.Center

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 0

                        // Login type buttons
                        RowLayout {
                            Layout.preferredHeight: 35
                            Layout.topMargin: 22
                            Layout.alignment: Qt.AlignHCenter
                            spacing: 8

                            LPCLButton {
                                id: msBtn
                                Layout.preferredWidth: 80
                                padding: 5
                                radius: height / 2
                                colorType: loginType === 5 ? 1 : 0
                                hasBorder: false
                                contentItem: Row {
                                    spacing: 6
                                    LPCLIcon {
                                        size: 16
                                        lucideIcon: "shield-check"
                                        anchors.verticalCenter: parent.verticalCenter
                                        iconColor: msBtn.labelColor
                                    }
                                    Text {
                                        text: "正版"
                                        color: msBtn.labelColor
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSize
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                                onClicked: {
                                    loginType = 5;
                                }
                            }
                            LPCLButton {
                                id: offlineBtn
                                Layout.preferredWidth: 80
                                padding: 5
                                radius: height / 2
                                colorType: loginType === 0 ? 1 : 0
                                hasBorder: false
                                contentItem: Row {
                                    spacing: 6
                                    LPCLIcon {
                                        size: 16
                                        lucideIcon: "unlink"
                                        anchors.verticalCenter: parent.verticalCenter
                                        iconColor: offlineBtn.labelColor
                                    }
                                    Text {
                                        text: "离线"
                                        color: offlineBtn.labelColor
                                        font.family: Theme.fontFamily
                                        font.pixelSize: Theme.fontSize
                                        anchors.verticalCenter: parent.verticalCenter
                                    }
                                }
                                onClicked: {
                                    loginType = 0;
                                }
                            }
                        }

                        Item {
                            Layout.fillHeight: true
                        }

                        // Avatar + username
                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 110

                            // Avatar
                            LPCLIcon {
                                id: avatarIcon
                                anchors {
                                    horizontalCenter: parent.horizontalCenter
                                    top: parent.top
                                }
                                size: 50
                                assetsIcon: "Steve"
                            }

                            // Username input
                            LPCLTextBox {
                                anchors {
                                    horizontalCenter: parent.horizontalCenter
                                    top: avatarIcon.bottom
                                    topMargin: 20
                                }
                                width: parent.width - 40
                                placeholderText: "游戏用户名"
                                text: accountName || ""
                                onTextChanged: {
                                    accountName = text;
                                }
                            }
                        }

                        // Spacer
                        Item {
                            Layout.fillHeight: true
                        }

                        // Launch button + version status
                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 75

                            LPCLButton {
                                id: btnLaunch
                                anchors {
                                    left: parent.left
                                    right: parent.right
                                    top: parent.top
                                }
                                anchors.leftMargin: 20
                                anchors.rightMargin: 20
                                height: Theme.launchBtnHeight
                                colorType: (btnLaunchState === 3 || btnLaunchState === 2) ? 1 : 3
                                text: btnLaunchText
                                enabled: btnLaunchState >= 2
                                padding: 0
                                onClicked: launchButtonClick()
                            }
                        }

                        // Version select + version settings
                        RowLayout {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 45
                            Layout.leftMargin: 20
                            Layout.rightMargin: 20

                            LPCLButton {
                                id: btnVersion
                                Layout.fillWidth: true
                                Layout.preferredHeight: Theme.versionBtnHeight
                                colorType: 0
                                text: "版本选择"   // Original always shows "版本选择"
                                enabled: btnLaunchState !== 0
                                onClicked: pushSubPage("版本选择")
                            }
                            LPCLButton {
                                id: btnMore
                                Layout.preferredWidth: 80
                                Layout.preferredHeight: Theme.versionBtnHeight
                                colorType: 0
                                text: "版本设置"
                                visible: btnLaunchState === 3
                                onClicked: { /* TODO: Navigate to version setup */ }
                            }
                        }

                        Item {
                            Layout.preferredHeight: 20
                        }
                    }
                }

                // PanLaunching — Launch progress overlay
                Item {
                    id: panLaunching
                    anchors.fill: parent
                    visible: panLaunchingOpacity > 0
                    opacity: panLaunchingOpacity
                    scale: panLaunchingScale
                    transformOrigin: Item.Center

                    // Content centered vertically (matching original Grid with * rows)
                    ColumnLayout {
                        anchors.centerIn: parent
                        width: parent.width - 40
                        spacing: 0

                        Item {
                            Layout.alignment: Qt.AlignHCenter
                            Layout.preferredWidth: 50
                            Layout.preferredHeight: 50
                            Layout.topMargin: 10
                            LPCLProgressBar {
                                anchors.fill: parent
                                indeterminate: Launcher.launchState < Launcher.Downloading
                            }
                        }

                        Text {
                            id: launchTitleText
                            text: panLaunchingTitle
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeLaunchTitle
                            color: Theme.color3
                            Layout.alignment: Qt.AlignHCenter
                            Layout.topMargin: 10
                            transform: Rotation {
                                angle: -3
                                origin.x: launchTitleText.width / 2
                                origin.y: launchTitleText.height / 2
                            }
                        }
                        Text {
                            id: launchNameText
                            text: selectedVersion
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeLaunchName
                            color: Theme.color3
                            Layout.alignment: Qt.AlignHCenter
                            Layout.topMargin: 5
                            Layout.bottomMargin: 12
                            transform: Rotation {
                                angle: -3
                                origin.x: launchNameText.width / 2
                                origin.y: launchNameText.height / 2
                            }
                        }

                        // Progress bar (margin 30,12,30,27 matching original)
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: Theme.progressBarHeight
                            Layout.leftMargin: 10
                            Layout.rightMargin: 10
                            Layout.topMargin: 12
                            Layout.bottomMargin: 27
                            radius: 0
                            color: Theme.gray6
                            opacity: 0.6
                            Rectangle {
                                width: parent.width * (showProgress / 100.0)
                                height: parent.height
                                gradient: Gradient {
                                    GradientStop {
                                        position: 0
                                        color: Theme.color4
                                    }
                                    GradientStop {
                                        position: 0.6
                                        color: Theme.color3
                                    }
                                }
                                Behavior on width {
                                    NumberAnimation {
                                        duration: 130
                                    }
                                }
                            }
                        }

                        // Status info grid (matching PanLaunchingInfo)
                        GridLayout {
                            Layout.alignment: Qt.AlignHCenter
                            columns: 2
                            rowSpacing: 5
                            columnSpacing: 15
                            Text {
                                text: "当前步骤"
                                font: infoFont
                                color: Theme.gray3
                                opacity: 0.5
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: 80
                            }
                            Text {
                                text: Launcher.statusText
                                font: infoFont
                                color: Theme.color3
                                Layout.preferredWidth: 100
                            }
                            Text {
                                text: "登录方式"
                                font: infoFont
                                color: Theme.gray3
                                opacity: 0.5
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: 80
                            }
                            Text {
                                text: loginType === 5 ? "登录" : "离线登录"
                                font: infoFont
                                color: Theme.color3
                                Layout.preferredWidth: 100
                            }
                            Text {
                                text: "启动进度"
                                font: infoFont
                                color: Theme.gray3
                                opacity: 0.5
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: 80
                            }
                            Text {
                                text: showProgress.toFixed(2) + " %"
                                font: infoFont
                                color: Theme.color3
                                Layout.preferredWidth: 100
                            }
                            Text {
                                text: "下载速度"
                                font: infoFont
                                color: Theme.gray3
                                opacity: 0.5
                                horizontalAlignment: Text.AlignRight
                                Layout.preferredWidth: 80
                                visible: false
                            }
                            Text {
                                text: ""
                                font: infoFont
                                color: Theme.color3
                                Layout.preferredWidth: 100
                                visible: false
                            }
                        }
                    }

                    // Cancel button at bottom (full-width, matching original Margin="20,0,20,20" Height=35)
                    LPCLButton {
                        anchors {
                            left: parent.left
                            right: parent.right
                            bottom: parent.bottom
                        }
                        anchors.leftMargin: 20
                        anchors.rightMargin: 20
                        anchors.bottomMargin: 20
                        height: Theme.versionBtnHeight
                        text: isRunning ? "结束游戏" : "取消"
                        colorType: 2
                        onClicked: Launcher.interrupt()
                    }
                }

                // Version selection sub-page (replaces sidebar content)
                // Matching original PageSelectLeft + PageSelectRight navigation
                Item {
                    id: versionSelectView
                    anchors.fill: parent
                    visible: isSubPage
                    opacity: isSubPage ? 1 : 0
                    Behavior on opacity {
                        NumberAnimation {
                            duration: 100
                        }
                    }

                    // Loading spinner overlay
                    Rectangle {
                        anchors.fill: parent
                        color: Theme.pureWhite
                        visible: VersionManager.isLoading
                        z: 1

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 12
                            LPCLProgressBar {
                                Layout.preferredWidth: 50
                                Layout.preferredHeight: 50
                                indeterminate: true
                            }
                            Text {
                                text: "正在扫描版本..."
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSize
                                color: Theme.gray3
                                Layout.alignment: Qt.AlignHCenter
                            }
                        }
                    }

                    // Version list (matching original PageSelectRight layout)
                    Flickable {
                        anchors.fill: parent
                        visible: !VersionManager.isLoading
                        contentWidth: width
                        contentHeight: verCol.implicitHeight + 20
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: LPCLScrollBar {}

                        ColumnLayout {
                            id: verCol
                            anchors {
                                left: parent.left
                                right: parent.right
                                top: parent.top
                            }
                            spacing: 2

                            // Section header
                            Text {
                                text: "常规版本"
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSmall
                                color: Theme.gray3
                                opacity: 0.6
                                Layout.leftMargin: 15
                                Layout.topMargin: 12
                                Layout.bottomMargin: 4
                            }

                            Repeater {
                                model: VersionManager.versionIds

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 40
                                    Layout.leftMargin: 5
                                    Layout.rightMargin: 5
                                    radius: Theme.buttonRadius
                                    color: verMouse.containsMouse ? Theme.color7 : "transparent"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.leftMargin: 12
                                        anchors.rightMargin: 8
                                        spacing: 10

                                        Rectangle {
                                            width: 24
                                            height: 24
                                            radius: 3
                                            color: Theme.color6
                                            Layout.alignment: Qt.AlignVCenter
                                            Text {
                                                anchors.centerIn: parent
                                                text: "⛏"
                                                font.pixelSize: 14
                                            }
                                        }
                                        Text {
                                            text: modelData
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSize
                                            color: Theme.color1
                                            Layout.fillWidth: true
                                            elide: Text.ElideRight
                                        }
                                    }

                                    MouseArea {
                                        id: verMouse
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            selectedVersion = modelData;
                                            refreshButtonsUI();
                                            popSubPage();
                                        }
                                    }
                                }
                            }

                            Item {
                                Layout.preferredHeight: 10
                            }
                        }
                    }

                    // Back button overlay (top-right corner, matching original PageBack)
                    LPCLButton {
                        anchors {
                            right: parent.right
                            top: parent.top
                            margins: 10
                        }
                        width: 60
                        height: 30
                        text: "← 返回"
                        colorType: 0
                        z: 5
                        onClicked: popSubPage()
                    }
                }
            }
        }

        // ---- Right content ----
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"

            Flickable {
                anchors.fill: parent
                contentWidth: width
                contentHeight: panMain.implicitHeight + 25
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: LPCLScrollBar {}

                ColumnLayout {
                    id: panMain
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: parent.top
                    }
                    anchors.margins: 25
                    spacing: 15

                    // Quick launch card
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: quickCardCol.height + 36
                        radius: Theme.buttonRadius
                        color: Theme.pureWhite
                        border {
                            width: 1
                            color: Theme.gray5
                        }

                        ColumnLayout {
                            id: quickCardCol
                            anchors {
                                left: parent.left
                                right: parent.right
                                top: parent.top
                            }
                            anchors.margins: 20
                            spacing: 12

                            Text {
                                text: "快速启动"
                                font: boldFont
                                color: Theme.color1
                            }
                            Text {
                                text: "版本: " + (selectedVersion || "未选择") + "\n目录: " + VersionManager.mcFolder
                                font: smallFont
                                color: Theme.gray3
                                Layout.fillWidth: true
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    // Log card
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 150
                        radius: Theme.buttonRadius
                        color: Theme.pureWhite
                        border {
                            width: 1
                            color: Theme.gray5
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 38
                                color: "transparent"
                                Text {
                                    anchors {
                                        left: parent.left
                                        top: parent.top
                                        leftMargin: 20
                                        topMargin: 18
                                    }
                                    text: "启动日志"
                                    font: boldFont
                                    color: Theme.color1
                                }
                            }
                            Flickable {
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                Layout.leftMargin: 20
                                Layout.rightMargin: 23
                                Layout.bottomMargin: 18
                                clip: true
                                contentWidth: width
                                contentHeight: labLog.implicitHeight
                                ScrollBar.vertical: LPCLScrollBar {}
                                Text {
                                    id: labLog
                                    width: parent.width
                                    text: "LPCL v" + Qt.application.version + "\nReady.\n"
                                    color: Theme.color1
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSize
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }

            Connections {
                target: Launcher
                function onGameLog(line) {
                    labLog.text += line + "\n";
                }
            }
        }
    }

    // ---- MS OAuth ----
    MsAuth {
        id: msAuth
        onDeviceCodeReady: function (code, url) {
            msDeviceCode = code;
            msVerifyUrl = url;
        }
        onLoginProgress: function (status) {
            statusText = status;
        }
        onLoginFinished: function (success, result) {
            msPolling = false;
            if (success) {
                accountName = result.name;
                accountUuid = result.uuid;
                loginResult = result;
                statusText = "已登录: " + accountName;
            } else {
                loginType = 0;
            }
        }
    }

    // ---- Launch button click (matching LaunchButtonClick) ----
    function launchButtonClick() {
        if (btnLaunchState === 2) { /* TODO: Navigate to download page */
            return;
        }
        if (btnLaunchState !== 3)
            return;
        doLaunch();
    }

    function doLaunch() {
        if (!selectedVersion)
            return;
        // Supply credentials to the launcher. Build an offline login if no
        // prior login result exists (e.g. user never went through MS auth).
        var login = loginResult;
        if (login === null)
            login = OfflineAuth.createOfflineLogin(accountName);
        Launcher.launchVersion(selectedVersion, login);
    }

    function startMsLogin() {
        msPolling = true;
        msAuth.startLogin();
    }

    function refreshVersions() {
        VersionManager.loadLocalVersions();
    }

    // ---- Fonts ----
    readonly property font smallFont: Qt.font({
        family: Theme.fontFamily,
        pixelSize: Theme.fontSizeSmall
    })
    readonly property font infoFont: Qt.font({
        family: Theme.fontFamily,
        pixelSize: Theme.fontSizeLaunchLabel
    })
    readonly property font boldFont: Qt.font({
        family: Theme.fontFamily,
        pixelSize: Theme.fontSizeLaunchName,
        bold: true
    })

    // ---- Keyboard shortcuts ----
    Keys.onReturnPressed: {
        if (isActive && !isSubPage)
            launchButtonClick();
    }
    Keys.onEscapePressed: {
        if (isSubPage)
            popSubPage();
    }

    // ---- Initialize ----
    Component.onCompleted: {
        refreshButtonsUI();
    }

    // ---- Persistent DropArea (secondary pathway for external file drag-and-drop) ----
    DropArea {
        id: pageDropArea
        anchors.fill: parent
        z: 998
        keys: ["text/uri-list"]
        onEntered: function (drag) {
            if (page.isActive && drag.hasUrls) {
                page.dragHovering = true;
                drag.acceptProposedAction();
            }
        }
        onExited: {
            page.dragHovering = false;
        }
        onDropped: function (drop) {
            if (!page.isActive)
                return;
            var urls = drop.urls;
            var files = [];
            for (var i = 0; i < urls.length; i++) {
                files.push(urls[i].toString().replace(/^(file:\/\/)/i, ''));
            }
            if (files.length > 0) {
                page.handleFilesDropped(files);
            }
            drop.acceptProposedAction();
        }
    }

    // ---- Drag-and-drop visual overlay (appears when external files are dragged over this page) ----
    Rectangle {
        id: dragOverlay
        anchors.fill: parent
        z: 999
        radius: Theme.buttonRadius
        color: Qt.rgba(0, 0, 0, 0.2)
        visible: dragHovering
        opacity: dragHovering ? 1 : 0
        Behavior on opacity {
            NumberAnimation {
                duration: 150
            }
        }

        // Dashed-border effect via a second rounded rect slightly inset
        Rectangle {
            anchors.fill: parent
            anchors.margins: 4
            radius: Theme.buttonRadius - 2
            color: "transparent"
        }

        ColumnLayout {
            anchors.centerIn: parent
            spacing: 10
            LPCLIcon {
                Layout.alignment: Qt.AlignHCenter
                size: 48
                lucideIcon: "file-plus"
                iconColor: Theme.color2
                opacity: 0.7
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "释放文件以识别"
                font.family: Theme.fontFamily
                font.pixelSize: Theme.fontSizeLaunchName
                font.bold: true
                color: Theme.color2
                opacity: 0.8
            }
        }

        // Dismiss on click-through
        MouseArea {
            anchors.fill: parent
            onClicked: dragHovering = false
        }
    }
}
