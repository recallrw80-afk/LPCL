import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PCL.Core
import "../components"
import "../styles"

// Launch tab — left sidebar + right content
Item {
    Rectangle { anchors.fill: parent; color: "#ff0000"; opacity: 0.15 }  // DEBUG sizing
    RowLayout {
        anchors.fill: parent
        spacing: 0

    // ========================================================================
    // Left sidebar — from PageLaunchLeft.qml
    // ========================================================================
    Rectangle {
        Layout.preferredWidth: 300
        Layout.fillHeight: true
        color: Theme.sidebarBg

        // Right shadow edge
        Rectangle {
            anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
            width: 4
            opacity: 0.04
            gradient: Gradient {
                GradientStop { position: 0; color: "#000000" }
                GradientStop { position: 1; color: "#00000000" }
            }
        }

        Item {
            id: panBack
            anchors.fill: parent
            clip: true

            property bool isLaunching: false
            property string versionName: "正在加载版本列表，请稍候"
            property string versionType: ""
            property bool versionReady: false
            property bool isOfflineLogin: false

            // PanInput — Version selection mode
            Item {
                id: panInput
                anchors.fill: parent
                visible: !panBack.isLaunching
                opacity: panBack.isLaunching ? 0 : 1
                Behavior on opacity { NumberAnimation { duration: 200 } }

                ColumnLayout {
                    anchors { fill: parent; leftMargin: 20; rightMargin: 20; topMargin: 0; bottomMargin: 0 }
                    spacing: 0

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 60
                        Text {
                            anchors.centerIn: parent
                            text: panBack.isOfflineLogin ? "离线模式" : "正版登录"
                            color: Theme.color3
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeTitle
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        Layout.topMargin: 24
                        Layout.bottomMargin: -4
                        Rectangle {
                            anchors.centerIn: parent
                            width: tagRow.implicitWidth + 30
                            height: 27
                            radius: 13
                            color: Theme.semiTransparent
                            Row {
                                id: tagRow
                                anchors.centerIn: parent
                                spacing: 10
                                Rectangle {
                                    width: 16; height: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    radius: 8
                                    color: Theme.color3
                                    visible: panBack.versionType !== ""
                                }
                                Text {
                                    text: panBack.versionType || "选择版本"
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 14
                                    color: Theme.color3
                                    anchors.verticalCenter: parent.verticalCenter
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    Row {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 35
                        Layout.topMargin: 22
                        Layout.alignment: Qt.AlignHCenter
                        spacing: 8
                        MyButton {
                            width: 100; height: 28
                            text: "正版"
                            colorType: panBack.isOfflineLogin ? 0 : 1
                            onClicked: panBack.isOfflineLogin = false
                        }
                        MyButton {
                            width: 100; height: 28
                            text: "离线"
                            colorType: panBack.isOfflineLogin ? 1 : 0
                            onClicked: panBack.isOfflineLogin = true
                        }
                    }

                    Item { Layout.preferredHeight: 15 }

                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 70
                        MyButton {
                            id: btnLaunch
                            anchors { horizontalCenter: parent.horizontalCenter; top: parent.top }
                            width: parent.width
                            height: Theme.launchBtnHeight
                            text: panBack.versionReady ? "启动游戏" : "正在加载"
                            colorType: 1
                            enabled: panBack.versionReady
                            onClicked: panBack.isLaunching = true
                        }
                        Text {
                            id: labVersion
                            anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom }
                            text: panBack.versionName
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeXSmall
                            color: Theme.gray3
                            elide: Text.ElideRight
                        }
                    }

                    Item { Layout.preferredHeight: 20 }
                }
            }

            // PanLaunching — Launch progress mode
            Item {
                id: panLaunching
                anchors.fill: parent
                visible: panBack.isLaunching
                opacity: panBack.isLaunching ? 1 : 0
                scale: panBack.isLaunching ? 1.0 : 0.8
                Behavior on opacity { NumberAnimation { duration: 200 } }
                Behavior on scale { NumberAnimation { duration: 200 } }

                ColumnLayout {
                    anchors { fill: parent; leftMargin: 20; rightMargin: 20; bottomMargin: 0 }
                    spacing: 0

                    Item { Layout.fillHeight: true }

                    Item {
                        Layout.alignment: Qt.AlignCenter
                        Layout.preferredWidth: 50; Layout.preferredHeight: 50
                        Layout.bottomMargin: 10
                        Rectangle {
                            anchors.centerIn: parent
                            width: 30; height: 30; radius: 15
                            color: Theme.color4
                            RotationAnimation on rotation {
                                from: 0; to: 360
                                duration: 1500
                                loops: Animation.Infinite
                            }
                        }
                    }

                    Text {
                        Layout.alignment: Qt.AlignCenter
                        text: "正在启动游戏"
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLaunchTitle
                        color: Theme.color3
                        transform: Matrix4x4 {
                            property real skewAngle: -3 * Math.PI / 180
                            matrix: Qt.matrix4x4(1, Math.tan(skewAngle), 0, 0,
                                                 0, 1, 0, 0,
                                                 0, 0, 1, 0,
                                                 0, 0, 0, 1)
                        }
                    }

                    Text {
                        Layout.alignment: Qt.AlignCenter
                        Layout.topMargin: 5
                        Layout.leftMargin: 40; Layout.rightMargin: 40
                        text: panBack.versionName
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontSizeLaunchName
                        color: Theme.color3
                        elide: Text.ElideRight
                        transform: Matrix4x4 {
                            property real skewAngle: -3 * Math.PI / 180
                            matrix: Qt.matrix4x4(1, Math.tan(skewAngle), 0, 0,
                                                 0, 1, 0, 0,
                                                 0, 0, 1, 0,
                                                 0, 0, 0, 1)
                        }
                    }

                    RowLayout {
                        Layout.alignment: Qt.AlignCenter
                        Layout.preferredWidth: parent.width
                        Layout.preferredHeight: 4
                        Layout.topMargin: 12
                        Layout.bottomMargin: 27
                        spacing: 0
                        Rectangle {
                            Layout.fillWidth: true; Layout.preferredWidth: 7; Layout.fillHeight: true
                            gradient: Gradient {
                                GradientStop { position: 0; color: Theme.color4 }
                                GradientStop { position: 0.6; color: Theme.color3 }
                            }
                        }
                        Rectangle {
                            Layout.fillWidth: true; Layout.preferredWidth: 3; Layout.fillHeight: true
                            color: Theme.color6; opacity: 0.6
                        }
                    }

                    GridLayout {
                        Layout.alignment: Qt.AlignCenter
                        Layout.preferredWidth: 260
                        columns: 4; rowSpacing: 5; columnSpacing: 0
                        Text { Layout.row: 0; Layout.column: 1; Layout.alignment: Qt.AlignRight; text: "当前步骤"; font.pixelSize: Theme.fontSizeLaunchLabel; color: Theme.color1; opacity: 0.5 }
                        Text { Layout.row: 0; Layout.column: 2; Layout.leftMargin: 15; text: "下载支持"; font.pixelSize: Theme.fontSizeLaunchLabel; color: Theme.color1 }
                        Text { Layout.row: 1; Layout.column: 1; Layout.alignment: Qt.AlignRight; text: "登录方式"; font.pixelSize: Theme.fontSizeLaunchLabel; color: Theme.color1; opacity: 0.5 }
                        Text { Layout.row: 1; Layout.column: 2; Layout.leftMargin: 15; text: panBack.isOfflineLogin ? "离线登录" : "正版登录"; font.pixelSize: Theme.fontSizeLaunchLabel; color: Theme.color1 }
                        Text { Layout.row: 2; Layout.column: 1; Layout.alignment: Qt.AlignRight; text: "启动进度"; font.pixelSize: Theme.fontSizeLaunchLabel; color: Theme.color1; opacity: 0.5 }
                        Text { Layout.row: 2; Layout.column: 2; Layout.leftMargin: 15; text: "69.28 %"; font.pixelSize: Theme.fontSizeLaunchLabel; color: Theme.color1 }
                    }

                    Item { Layout.fillHeight: true }

                    MyButton {
                        Layout.alignment: Qt.AlignCenter
                        Layout.preferredWidth: parent.width
                        Layout.preferredHeight: Theme.versionBtnHeight
                        Layout.bottomMargin: 20
                        text: "取消"
                        onClicked: panBack.isLaunching = false
                    }
                }
            }
        }
    }

    // ========================================================================
    // Right content — from PageLaunchRight.qml
    // ========================================================================
    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        color: "transparent"

        Flickable {
            id: flickable
            anchors.fill: parent
            contentWidth: width
            contentHeight: panMain.implicitHeight + 25
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: MyScrollBar {}

            ColumnLayout {
                id: panMain
                anchors { left: parent.left; right: parent.right; top: parent.top }
                anchors.margins: 25
                spacing: 15

                Rectangle {
                    id: panHint
                    Layout.fillWidth: true
                    Layout.preferredHeight: hintText.implicitHeight + 28
                    radius: Theme.buttonRadius
                    color: "transparent"
                    border.width: 0
                    visible: true
                    clip: true
                    Rectangle {
                        anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                        width: 4; color: Theme.color3; radius: 2
                    }
                    Rectangle {
                        anchors { left: parent.left; right: parent.right; top: parent.top; bottom: parent.bottom }
                        anchors.leftMargin: 4
                        color: Theme.color6; radius: Theme.buttonRadius
                    }
                    RowLayout {
                        anchors { fill: parent; margins: 8; leftMargin: 14 }
                        spacing: 8
                        Text {
                            id: hintText
                            text: "快照版 PCL 包含尚未正式发布的测试功能。\n请不要随意发给其他人。"
                            color: Theme.color3
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSize
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                        }
                        MyButton {
                            text: "×"
                            Layout.preferredWidth: 24; Layout.preferredHeight: 24
                            colorType: 0
                            onClicked: panHint.visible = false
                        }
                    }
                }

                Item {
                    id: panCustom
                    Layout.fillWidth: true
                    Layout.preferredHeight: 0
                }

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
                        Flickable {
                            id: logFlick
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.leftMargin: 20; Layout.rightMargin: 23; Layout.bottomMargin: 18
                            clip: true
                            contentWidth: width
                            contentHeight: labLog.implicitHeight
                            ScrollBar.vertical: MyScrollBar {}
                            Text {
                                id: labLog
                                width: parent.width
                                text: "LPCL v0.1\nReady.\n"
                                color: Theme.color1
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSize
                                wrapMode: Text.WordWrap
                                textFormat: Text.PlainText
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
}
