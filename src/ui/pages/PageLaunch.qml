import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PCL.Core
import "../components"
import "../styles"

// Launch tab — left sidebar + right content
Item {
    property bool isActive: false
    visible: opacity > 0
    opacity: isActive ? 1 : 0
    scale: isActive ? 1 : 0.96
    Behavior on opacity { NumberAnimation { duration: 100 } }
    Behavior on scale { NumberAnimation { duration: 400; easing.type: Easing.OutBack } }

    RowLayout {
        anchors.fill: parent
        spacing: 0

    // ---- Left sidebar ----
    Rectangle {
        Layout.preferredWidth: 300; Layout.fillHeight: true
        color: Theme.sidebarBg
        Rectangle {
            anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
            width: 4; opacity: 0.04
            gradient: Gradient {
                GradientStop { position: 0; color: "#000000" }
                GradientStop { position: 1; color: "#00000000" }
            }
        }

        Item {
            anchors.fill: parent;
            clip: true
            ColumnLayout {
                anchors.fill: parent
                spacing: 0
                RowLayout {
                    Layout.preferredHeight: 35
                    Layout.topMargin: 22
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 8

                    LPCLButton{
                        id: shildButton
                        padding: 15
                        radius: height / 2
                        contentItem: Row {
                            spacing: 6
                            LPCLIcon {
                                size: 16
                                iconColor: Theme.color3
                                lucideIcon: "shield-check"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: "正版"
                                color: Theme.color3
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSize
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                    LPCLButton{
                        id: unlinkButton
                        padding: 15
                        radius: height / 2
                        contentItem: Row {
                            spacing: 6
                            LPCLIcon {
                                size: 16
                                iconColor: Theme.color3
                                lucideIcon: "unlink"
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: "离线"
                                color: Theme.color3
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSize
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }
                    }
                }
                Item {
                    Layout.fillWidth: true; Layout.preferredHeight: 60
                    Text {
                        anchors.centerIn: parent
                        text: "正版登录"; color: Theme.color3
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeTitle
                    }
                }
                Item {
                    Layout.fillWidth: true; Layout.preferredHeight: 30
                    Layout.topMargin: 24; Layout.bottomMargin: -4
                    Rectangle {
                        anchors.centerIn: parent; width: 120; height: 27; radius: 13
                        color: Theme.semiTransparent
                        Text {
                            anchors.centerIn: parent; text: "选择版本"
                            font.family: Theme.fontFamily; font.pixelSize: 14; color: Theme.color3
                        }
                    }
                }
                Item { Layout.fillHeight: true }
                Item { Layout.preferredHeight: 15 }
                Item {
                    Layout.fillWidth: true; Layout.preferredHeight: 70
                    LPCLButton {
                        anchors { horizontalCenter: parent.horizontalCenter; top: parent.top }
                        width: parent.width; height: Theme.launchBtnHeight
                        text: "启动游戏"; colorType: 1
                    }
                    Text {
                        anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom }
                        text: "正在加载版本列表，请稍候"
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeXSmall
                        color: Theme.gray3
                    }
                }
                Item { Layout.preferredHeight: 20 }
            }
        }
    }

    // ---- Right content ----
    Rectangle {
        Layout.fillWidth: true; Layout.fillHeight: true; color: "transparent"
        Flickable {
            anchors.fill: parent
            contentWidth: width; contentHeight: panMain.implicitHeight + 25
            clip: true; boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: LPCLScrollBar {}
            ColumnLayout {
                id: panMain
                anchors { left: parent.left; right: parent.right; top: parent.top }
                anchors.margins: 25; spacing: 15
                Rectangle {
                    Layout.fillWidth: true; Layout.fillHeight: true
                    Layout.minimumHeight: 150
                    radius: Theme.buttonRadius; color: Theme.pureWhite
                    border { width: 1; color: Theme.gray5 }
                    ColumnLayout {
                        anchors.fill: parent; spacing: 0
                        Rectangle {
                            Layout.fillWidth: true; Layout.preferredHeight: 38
                            color: "transparent"
                            Text {
                                anchors { left: parent.left; top: parent.top; leftMargin: 20; topMargin: 18 }
                                text: "启动日志"; color: Theme.color1
                                font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeLaunchName; font.bold: true
                            }
                        }
                        Flickable {
                            Layout.fillWidth: true; Layout.fillHeight: true
                            Layout.leftMargin: 20; Layout.rightMargin: 23; Layout.bottomMargin: 18
                            clip: true; contentWidth: width; contentHeight: labLog.implicitHeight
                            ScrollBar.vertical: LPCLScrollBar {}
                            Text {
                                id: labLog; width: parent.width
                                text: "LPCL v0.1\nReady.\n"
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
}
