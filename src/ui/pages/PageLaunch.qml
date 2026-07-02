import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PCL.Core
import "../components"
import "../styles"

// Launch tab — replica of PageLaunchLeft.xaml + PageLaunchRight.xaml
// Two modes: PanInput (default) + PanLaunching (during game launch)
Item {
    property bool isActive: false
    property bool isLaunching: Launcher.launchState === LaunchState.Loading
    visible: opacity > 0
    opacity: isActive ? 1 : 0
    scale: isActive ? 1 : 0.96
    Behavior on opacity { NumberAnimation { duration: 100 } }
    Behavior on scale { NumberAnimation { duration: 400; easing.type: Easing.OutBack } }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ================================================================
        // Left sidebar — replica of PageLaunchLeft.xaml PanInput + PanLaunching
        // ================================================================
        Rectangle {
            Layout.preferredWidth: 280
            Layout.fillHeight: true
            color: Theme.sidebarBg

            // Right edge shadow
            Rectangle {
                anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
                width: 4; opacity: 0.04
                gradient: Gradient {
                    GradientStop { position: 0; color: "#000000" }
                    GradientStop { position: 1; color: "#00000000" }
                }
            }

            // ---- PanInput: login + version + launch button ----
            Item {
                id: panInput
                anchors.fill: parent
                visible: !isLaunching

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
                            id: shildButton
                            padding: 15
                            radius: height / 2
                            colorType: loginType === 5 ? 1 : 0
                            contentItem: Row {
                                spacing: 6
                                LPCLIcon {
                                    size: 16
                                    iconColor: loginType === 5 ? "white" : Theme.color3
                                    lucideIcon: "shield-check"
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: "正版"
                                    color: loginType === 5 ? "white" : Theme.color3
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSize
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                            onClicked: loginType = 5
                        }
                        LPCLButton {
                            id: unlinkButton
                            padding: 15
                            radius: height / 2
                            colorType: loginType === 0 ? 1 : 0
                            contentItem: Row {
                                spacing: 6
                                LPCLIcon {
                                    size: 16
                                    iconColor: loginType === 0 ? "white" : Theme.color3
                                    lucideIcon: "unlink"
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                                Text {
                                    text: "离线"
                                    color: loginType === 0 ? "white" : Theme.color3
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSize
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                            onClicked: loginType = 0
                        }
                    }

                    // Account display area
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 60
                        Text {
                            anchors.centerIn: parent
                            text: accountName || (loginType === 0 ? "离线登录" : "正版登录")
                            color: Theme.color3
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeTitle
                        }
                    }

                    // Version selector pill
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        Layout.topMargin: 24
                        Layout.bottomMargin: -4

                        Rectangle {
                            anchors.centerIn: parent
                            width: Math.min(verPillRow.implicitWidth + 24, parent.width - 40)
                            height: 27
                            radius: 13
                            color: Theme.semiTransparent

                            Row {
                                id: verPillRow
                                anchors.centerIn: parent
                                spacing: 8
                                LPCLIcon {
                                    size: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    lucideIcon: "layout-grid"
                                    iconColor: Theme.color3
                                    visible: selectedVersion !== ""
                                }
                                Text {
                                    text: selectedVersion || "选择版本"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 14
                                    color: Theme.color3
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: versionDropdown.open()
                            }
                        }
                    }

                    // Version dropdown popup
                    LPCLComboBox {
                        id: versionDropdown
                        visible: false
                        model: VersionManager.versionIds
                        onActivated: index => {
                            selectedVersion = VersionManager.versionIds[index]
                        }
                    }

                    // Spacer
                    Item { Layout.fillHeight: true }

                    // Launch button area
                    Item { Layout.preferredHeight: 15 }
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 70

                        LPCLButton {
                            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top }
                            width: parent.width - 40
                            height: Theme.launchBtnHeight
                            text: "启动游戏"
                            colorType: 1
                            enabled: selectedVersion !== ""
                            onClicked: {
                                isLaunching_ = true
                                Launcher.launch(selectedVersion)
                            }
                        }
                        Text {
                            anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom }
                            text: statusText
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXSmall
                            color: Theme.gray3
                        }
                    }
                    Item { Layout.preferredHeight: 20 }
                }
            }

            // ---- PanLaunching: launch progress overlay ----
            Item {
                id: panLaunching
                anchors.fill: parent
                visible: isLaunching
                opacity: isLaunching ? 1 : 0
                scale: isLaunching ? 1 : 0.8
                Behavior on opacity { NumberAnimation { duration: 250 } }
                Behavior on scale { NumberAnimation { duration: 400; easing.type: Easing.OutBack } }

                ColumnLayout {
                    anchors.centerIn: parent
                    width: parent.width - 40
                    spacing: 0

                    // Loading spinner
                    LPCLProgressBar {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 50
                        Layout.topMargin: 10
                        Layout.bottomMargin: 5
                        indeterminate: true
                    }

                    Text {
                        text: "正在启动游戏"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLaunchTitle
                        color: Theme.color3
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 10
                    }
                    Text {
                        text: selectedVersion
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLaunchName
                        color: Theme.color3
                        Layout.alignment: Qt.AlignHCenter
                        Layout.topMargin: 5
                        Layout.bottomMargin: 12
                    }

                    // Progress bar
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 4
                        Layout.topMargin: 12
                        Layout.bottomMargin: 27
                        radius: 2
                        color: Theme.gray6
                        opacity: 0.6

                        Rectangle {
                            width: parent.width * (launchProgress / 100)
                            height: parent.height
                            radius: 2
                            gradient: Gradient {
                                GradientStop { position: 0; color: Theme.color4 }
                                GradientStop { position: 0.6; color: Theme.color3 }
                            }
                        }
                    }

                    // Status info grid
                    GridLayout {
                        Layout.alignment: Qt.AlignHCenter
                        columns: 2
                        rowSpacing: 5
                        columnSpacing: 15
                        Layout.topMargin: 10

                        Text { text: "当前步骤"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeLaunchLabel; color: Theme.gray3; opacity: 0.5; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 80 }
                        Text { text: launchStage; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeLaunchLabel; color: Theme.color3; Layout.preferredWidth: 100 }
                        Text { text: "启动进度"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeLaunchLabel; color: Theme.gray3; opacity: 0.5; horizontalAlignment: Text.AlignRight; Layout.preferredWidth: 80 }
                        Text { text: launchProgress.toFixed(1) + " %"; font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeLaunchLabel; color: Theme.color3; Layout.preferredWidth: 100 }
                    }
                }
            }
        }

        // ================================================================
        // Right content — custom home cards + launch log
        // ================================================================
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
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    anchors.margins: 25
                    spacing: 15

                    // ---- Version info card ----
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: verCardContent.height + 36
                        radius: Theme.buttonRadius
                        color: Theme.pureWhite
                        border { width: 1; color: Theme.gray5 }
                        visible: selectedVersion !== ""

                        ColumnLayout {
                            id: verCardContent
                            anchors { left: parent.left; right: parent.right; top: parent.top }
                            anchors.margins: 20
                            spacing: 6
                            Text {
                                text: selectedVersion || ""
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeLaunchName
                                font.bold: true
                                color: Theme.color1
                            }
                            Text {
                                text: "Launch folder: " + VersionManager.mcFolder
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSizeSmall
                                color: Theme.gray3
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }

                    // ---- Launch log card ----
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 150
                        radius: Theme.buttonRadius
                        color: Theme.pureWhite
                        border { width: 1; color: Theme.gray5 }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 0

                            // Log header
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 38
                                color: "transparent"
                                Text {
                                    anchors { left: parent.left; top: parent.top; leftMargin: 20; topMargin: 18 }
                                    text: "启动日志"
                                    color: Theme.color1
                                    font.family: Theme.fontFamily
                                    font.pixelSize: Theme.fontSizeLaunchName
                                    font.bold: true
                                }
                            }

                            // Log content
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
                function onGameLog(line) { labLog.text += line + "\n" }
            }
        }
    }

    // ---- Internal state ----
    property int loginType: 5  // 5=正版(Ms), 0=离线(Legacy)
    property string selectedVersion: ""
    property string accountName: ""
    property string statusText: "正在加载版本列表，请稍候"
    property string launchStage: "准备中"
    property real launchProgress: 0
    property bool isLaunching_: false

    // ---- Version list loading ----
    Connections {
        target: VersionManager
        function onVersionListChanged() {
            if (VersionManager.versionCount > 0) {
                statusText = "已找到 " + VersionManager.versionCount + " 个版本"
                if (!selectedVersion && VersionManager.versionIds.length > 0) {
                    selectedVersion = VersionManager.versionIds[0]
                }
            }
        }
    }

    // ---- Launch state tracking ----
    Connections {
        target: Launcher
        function onStatusTextChanged() {
            launchStage = Launcher.statusText
            statusText = launchStage
        }
    }
}
