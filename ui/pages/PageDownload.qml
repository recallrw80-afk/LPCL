import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

// Download tab — resource browser
// Sidebar: categories + filters | Right: scrollable resource grid
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
                contentHeight: panCat.implicitHeight + 20
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: LPCLScrollBar {}

                ColumnLayout {
                    id: panCat
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    spacing: 0

                    // Search box
                    Item {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 55
                        Layout.topMargin: 18
                        LPCLTextBox {
                            id: searchBox
                            anchors { left: parent.left; right: parent.right; leftMargin: 15; rightMargin: 15 }
                            placeholderText: "搜索资源..."
                        }
                    }

                    // Section: Source
                    Text {
                        text: "资源来源"
                        color: Theme.color1; opacity: 0.6
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSmall
                        Layout.leftMargin: 15; Layout.topMargin: 10; Layout.bottomMargin: 4
                    }
                    Repeater {
                        model: [
                            { name: "CurseForge", icon: "layout-grid" },
                            { name: "Modrinth", icon: "layout-grid" }
                        ]
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 34
                            color: catMouse.containsMouse ? Theme.color7 : "transparent"
                            radius: Theme.buttonRadius
                            Layout.leftMargin: 8; Layout.rightMargin: 8

                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                spacing: 10
                                LPCLIcon { size: 16; lucideIcon: modelData.icon; iconColor: Theme.color3 }
                                Text {
                                    text: modelData.name
                                    color: Theme.color1
                                    font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize
                                    Layout.fillWidth: true; elide: Text.ElideRight
                                }
                            }
                            MouseArea {
                                id: catMouse; anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                            }
                        }
                    }

                    // Section: Categories
                    Text {
                        text: "分类浏览"
                        color: Theme.color1; opacity: 0.6
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSmall
                        Layout.leftMargin: 15; Layout.topMargin: 18; Layout.bottomMargin: 4
                    }
                    Repeater {
                        model: [
                            { name: "模组 (Mods)", tag: "mods" },
                            { name: "资源包 (Resource Packs)", tag: "resourcepacks" },
                            { name: "光影 (Shaders)", tag: "shaders" },
                            { name: "整合包 (Modpacks)", tag: "modpacks" },
                            { name: "世界 (Worlds)", tag: "worlds" },
                            { name: "数据包 (Data Packs)", tag: "datapacks" }
                        ]
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 34
                            color: tagMouse.containsMouse ? Theme.color7 : "transparent"
                            radius: Theme.buttonRadius
                            Layout.leftMargin: 8; Layout.rightMargin: 8

                            Text {
                                anchors { left: parent.left; top: parent.top; leftMargin: 30; topMargin: 8 }
                                text: modelData.name
                                color: Theme.color1
                                font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize
                            }
                            MouseArea {
                                id: tagMouse; anchors.fill: parent; hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: { selectedCategory = modelData.tag }
                            }
                        }
                    }

                    Item { Layout.preferredHeight: 20 }
                }
            }
        }

        // ---- Right content: resource grid ----
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 25
                spacing: 15

                // Header
                RowLayout {
                    Text {
                        text: "资源下载"
                        color: Theme.color1
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeLogo; font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Text {
                        text: selectedCategory || "全部"
                        color: Theme.gray3
                        font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSmall
                    }
                }

                // Resource grid
                Flickable {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentWidth: width
                    contentHeight: resGrid.implicitHeight + 20
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: LPCLScrollBar {}

                    GridLayout {
                        id: resGrid
                        anchors { left: parent.left; right: parent.right; top: parent.top }
                        columns: Math.max(1, Math.floor(width / 260))
                        rowSpacing: 12
                        columnSpacing: 12

                        Repeater {
                            model: dummyResources

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 80
                                radius: Theme.buttonRadius
                                color: Theme.pureWhite
                                border { width: 1; color: Theme.gray5 }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 12

                                    // Icon placeholder
                                    Rectangle {
                                        Layout.preferredWidth: 56; Layout.preferredHeight: 56
                                        radius: 4; color: Theme.gray7
                                        LPCLIcon {
                                            anchors.centerIn: parent
                                            size: 24; lucideIcon: "layout-grid"; iconColor: Theme.gray4
                                        }
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: 4
                                        Text {
                                            text: modelData.name
                                            color: Theme.color1
                                            font.family: Theme.fontFamily; font.pixelSize: Theme.fontSize
                                            font.bold: true; elide: Text.ElideRight
                                            Layout.fillWidth: true
                                        }
                                        Text {
                                            text: modelData.desc
                                            color: Theme.gray3
                                            font.family: Theme.fontFamily; font.pixelSize: Theme.fontSizeSmall
                                            elide: Text.ElideRight; maximumLineCount: 2
                                            wrapMode: Text.WordWrap
                                            Layout.fillWidth: true
                                        }
                                    }

                                    LPCLButton {
                                        text: "安装"
                                        Layout.preferredWidth: 60
                                        Layout.preferredHeight: 28
                                        colorType: 1
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    property string selectedCategory: ""
    property var dummyResources: [
        { name: "OptiFine HD U G5", desc: "光影支持与性能优化模组，提升 FPS 并支持高清纹理" },
        { name: "Just Enough Items (JEI)", desc: "物品配方查询模组，查看合成表和用途" },
        { name: "JourneyMap", desc: "小地图与全屏地图模组，支持路径点标记" },
        { name: "Sodium", desc: "高性能渲染引擎，大幅提升帧率" },
        { name: "Iris Shaders", desc: "光影加载器，兼容 OptiFine 光影包" },
        { name: "Xaero's Minimap", desc: "轻量级小地图模组，支持路径点和实体雷达" },
        { name: "BetterFPS", desc: "性能优化模组合集" }
    ]
}
