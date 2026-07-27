pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import LPCL

// Download tab — 游戏安装与资源下载
// 左侧：分类导航（游戏安装默认选中） | 右侧：游戏安装页 / 其余分类“即将推出”占位
Item {
    id: page
    property bool isActive: false
    visible: opacity > 0
    opacity: isActive ? 1 : 0
    scale: isActive ? 1 : 0.96
    Behavior on opacity { NumberAnimation { duration: 100 } }
    Behavior on scale { NumberAnimation { duration: 400; easing.type: Easing.OutBack } }

    // ---- 导航状态 ----
    property int selectedNav: 0
    property var currentNavItem: null
    property var navModel: [
        { name: "游戏安装", icon: "arrow-down-to-line" },
        { name: "Mod", icon: "package" },
        { name: "整合包", icon: "file-archive" },
        { name: "材质包", icon: "image" },
        { name: "光影包", icon: "bolt" },
        { name: "数据包", icon: "file-code" }
    ]

    // ---- 版本数据状态 ----
    // remoteVersions: VersionManager.remoteVersionList() 的缓存
    // installedIds: 本地已装版本 id（远程清单合入后 versionIds 会混入远程条目，
    //               故仅在列表处于“仅本地”状态时快照，安装成功后手动补充）
    // sessionInstalled: 本次会话内由本页安装成功的版本 id
    property var remoteVersions: []
    property var filteredVersions: []
    property var installedIds: []
    property var sessionInstalled: []
    property string pendingInstallId: ""

    // C++ 单例属性集中绑定（qmllint 无法解析单例，按项目惯例成对 disable）
    // qmllint disable unqualified
    readonly property bool versionsLoading: VersionManager.isLoading
    readonly property bool installBusy: InstallBridge.busy
    readonly property string installProgressText: InstallBridge.progressText
    readonly property int installProgressPercent: InstallBridge.progressPercent
    // qmllint enable unqualified

    readonly property string selectedNavName: selectedNav >= 0
                                              && selectedNav < navModel.length ? navModel[selectedNav].name : ""
    readonly property string selectedNavIcon: selectedNav >= 0
                                              && selectedNav < navModel.length ? navModel[selectedNav].icon : "package"

    // ---- 导航单选（RadioBox 互斥由使用方维护） ----
    function selectNav(idx, item) {
        selectedNav = idx;
        if (currentNavItem !== null && currentNavItem !== item)
            currentNavItem.checked = false;
        currentNavItem = item;
    }

    // ---- 从 VersionManager 同步远程清单与本地已装快照 ----
    function syncFromManager() {
        // qmllint disable unqualified
        var remote = VersionManager.remoteVersionList();
        var ids = VersionManager.versionIds;
        // qmllint enable unqualified
        remoteVersions = remote;
        if (remote.length === 0) {
            // 远程清单未合入时 versionIds 即本地已装列表
            var merged = ids.slice();
            for (var i = 0; i < sessionInstalled.length; i++) {
                if (merged.indexOf(sessionInstalled[i]) < 0)
                    merged.push(sessionInstalled[i]);
            }
            installedIds = merged;
        }
        rebuildFiltered();
    }

    // ---- 按搜索词与类型开关重建展示列表 ----
    function rebuildFiltered() {
        var kw = searchBox.text.trim().toLowerCase();
        var out = [];
        for (var i = 0; i < remoteVersions.length; i++) {
            var v = remoteVersions[i];
            if (!chkSnapshots.checked && v.type !== "release")
                continue;
            if (kw !== "" && String(v.id).toLowerCase().indexOf(kw) < 0)
                continue;
            var dateText = String(v.releaseTime).split("T")[0];
            out.push({
                vid: String(v.id),
                info: (dateText !== "" ? dateText + " · " : "") + typeLabel(String(v.type)),
                installed: installedIds.indexOf(v.id) >= 0
            });
        }
        filteredVersions = out;
    }

    // ---- 版本类型中文标签 ----
    function typeLabel(t) {
        switch (t) {
        case "release": return "正式版";
        case "snapshot": return "快照";
        case "old_alpha": return "远古版 Alpha";
        case "old_beta": return "远古版 Beta";
        default: return t;
        }
    }

    // ---- 拉取远程版本清单 ----
    function refreshRemote() {
        // qmllint disable unqualified
        VersionManager.fetchVersionManifest();
        // qmllint enable unqualified
    }

    // ---- 安装确认 → InstallBridge ----
    function confirmInstall(vid) {
        if (installBusy)
            return;
        pendingInstallId = vid;
        // qmllint disable missing-property
        Window.window.showMsg({
            title: "安装游戏",
            text: "下载并安装 Minecraft " + vid + "？",
            button1: "安装",
            button2: "取消",
            callback: function (result) {
                if (result === 1) {
                    // qmllint disable unqualified
                    InstallBridge.installMcVersion(vid);
                    // qmllint enable unqualified
                }
            }
        });
        // qmllint enable missing-property
    }

    // ---- 安装成功后记入已装列表 ----
    function markInstalled(vid) {
        if (vid === "")
            return;
        if (sessionInstalled.indexOf(vid) < 0) {
            var s = sessionInstalled.slice();
            s.push(vid);
            sessionInstalled = s;
        }
        if (installedIds.indexOf(vid) < 0) {
            var m = installedIds.slice();
            m.push(vid);
            installedIds = m;
        }
        rebuildFiltered();
    }

    Connections {
        // qmllint disable unqualified
        target: VersionManager
        // qmllint enable unqualified
        function onVersionListChanged() { page.syncFromManager(); }
    }

    Connections {
        // qmllint disable unqualified
        target: InstallBridge
        // qmllint enable unqualified
        function onMcInstallFinished(ok, msg) {
            if (ok) {
                page.markInstalled(page.pendingInstallId);
                // qmllint disable missing-property
                Window.window.showHint("Minecraft " + page.pendingInstallId + " 安装完成", "success");
                // qmllint enable missing-property
            } else {
                // qmllint disable missing-property
                Window.window.showMsg({ title: "安装失败", text: msg, warn: true });
                // qmllint enable missing-property
            }
        }
    }

    // 进入页面时远程清单为空且不在加载 → 自动拉取
    onIsActiveChanged: {
        if (isActive && remoteVersions.length === 0 && !versionsLoading)
            refreshRemote();
    }

    Component.onCompleted: syncFromManager()

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ---- 左侧分类导航 ----
        Rectangle {
            Layout.preferredWidth: 280
            Layout.fillHeight: true
            color: Theme.sidebarBg

            Rectangle {
                anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
                width: Theme.shadowWidth; opacity: Theme.shadowOpacity
                gradient: Gradient {
                    GradientStop { position: 0; color: "#000000" }
                    GradientStop { position: 1; color: "#00000000" }
                }
            }

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Text {
                    text: "下载"
                    color: Theme.color1
                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeLogo; font.bold: true
                    Layout.leftMargin: 15; Layout.topMargin: 18; Layout.bottomMargin: 10
                }

                Repeater {
                    id: navRepeater
                    model: page.navModel

                    LPCLListItem {
                        id: navItem
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        Layout.leftMargin: 8
                        Layout.rightMargin: 8
                        title: navItem.modelData.name
                        lucideIcon: navItem.modelData.icon
                        checkType: 2
                        checked: navItem.index === 0
                        onClicked: page.selectNav(navItem.index, navItem)
                        Component.onCompleted: if (navItem.index === 0) page.currentNavItem = navItem
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        // ---- 右侧内容 ----
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"

            // ---- 游戏安装 ----
            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.contentMargin
                spacing: Theme.sectionSpacing
                visible: page.selectedNav === 0

                // 搜索 + 刷新
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Theme.itemSpacing

                    LPCLSearchBox {
                        id: searchBox
                        Layout.fillWidth: true
                        hintText: "搜索版本号..."
                        onTextChanged: page.rebuildFiltered()
                        onAccepted: page.rebuildFiltered()
                    }
                    LPCLButton {
                        text: "刷新"
                        Layout.preferredHeight: 40
                        enabled: !page.versionsLoading && !page.installBusy
                        onClicked: page.refreshRemote()
                    }
                }

                // 类型开关 + 版本计数
                RowLayout {
                    Layout.fillWidth: true

                    LPCLCheckBox {
                        id: chkSnapshots
                        text: "显示快照与远古版本"
                        onChanged: page.rebuildFiltered()
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: "共 " + page.filteredVersions.length + " 个版本"
                        color: Theme.gray3
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSmall
                    }
                }

                // 安装进度行（busy 时显示）
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 64
                    visible: page.installBusy
                    radius: Theme.buttonRadius
                    color: Theme.pureWhite
                    border { width: 1; color: Theme.gray5 }

                    ColumnLayout {
                        anchors { left: parent.left; right: parent.right; verticalCenter: parent.verticalCenter }
                        anchors.margins: 12
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            Text {
                                text: page.installProgressText
                                color: Theme.color1
                                font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }
                            Text {
                                text: page.installProgressPercent + "%"
                                color: Theme.gray3
                                font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSmall
                            }
                        }
                        LPCLProgressBar {
                            Layout.fillWidth: true
                            value: page.installProgressPercent / 100
                        }
                    }
                }

                // 版本列表
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Flickable {
                        anchors.fill: parent
                        visible: !page.versionsLoading && page.filteredVersions.length > 0
                        contentWidth: width
                        contentHeight: verCol.implicitHeight + 10
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: LPCLScrollBar {}

                        ColumnLayout {
                            id: verCol
                            anchors { left: parent.left; right: parent.right; top: parent.top }
                            spacing: 4

                            Repeater {
                                model: page.filteredVersions

                                LPCLListItem {
                                    id: versionItem
                                    required property var modelData
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 46
                                    title: versionItem.modelData.vid
                                    info: versionItem.modelData.installed ? "已安装" : versionItem.modelData.info
                                    lucideIcon: "arrow-down-to-line"
                                    checkType: versionItem.modelData.installed ? 0 : 1
                                    enabled: !page.installBusy
                                    onClicked: page.confirmInstall(versionItem.modelData.vid)
                                }
                            }
                        }
                    }

                    // 加载中占位
                    Text {
                        anchors.centerIn: parent
                        visible: page.versionsLoading
                        text: "正在获取版本列表..."
                        color: Theme.gray3
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize
                    }

                    // 空结果占位
                    Text {
                        anchors.centerIn: parent
                        visible: !page.versionsLoading && page.filteredVersions.length === 0
                        text: page.remoteVersions.length === 0
                              ? "尚未获取到版本列表，点击“刷新”重试" : "没有匹配的版本"
                        color: Theme.gray3
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize
                    }
                }
            }

            // ---- 其余分类：即将推出 ----
            ColumnLayout {
                anchors.centerIn: parent
                visible: page.selectedNav !== 0
                spacing: 10

                LPCLIcon {
                    Layout.alignment: Qt.AlignHCenter
                    size: 48
                    lucideIcon: page.selectedNavIcon
                    iconColor: Theme.gray4
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: page.selectedNavName
                    color: Theme.color1
                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeLarge; font.bold: true
                }
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "该分类即将推出，敬请期待"
                    color: Theme.gray3
                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSmall
                }
            }
        }
    }
}
