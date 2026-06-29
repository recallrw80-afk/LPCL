import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import Qt5Compat.GraphicalEffects
import PCL.Core
import "src/ui/components"
import "src/ui/pages"
import "src/ui/styles"

ApplicationWindow {
    id: window

    // ---- Window sizing: 850:500 ratio, 40% of screen short side ----
    readonly property real baseRatio: 850 / 500  // 1.7
    readonly property real screenShort: Math.min(Screen.width, Screen.height)
    readonly property real targetHeight: screenShort * 0.4
    readonly property real targetWidth: targetHeight * baseRatio

    width: targetWidth
    height: targetHeight
    minimumWidth: 850
    minimumHeight: 500
    visible: true
    title: "Plain Craft Launcher  "

    // Opacity driven by custom property for reliable animation (entrance fade-in)
    property real winOpacity: 0
    opacity: winOpacity

    // Frameless + transparent for custom chrome (WindowStyle="None" AllowsTransparency="True" Topmost="True")
    flags: Qt.FramelessWindowHint | Qt.Window
    color: "transparent"

    // ========================================================================
    // PanBack (Grid, Margin=10)
    // ========================================================================
    Item {
        id: panBack
        anchors.fill: parent
        anchors.margins: Theme.windowMargin

        // Entrance animation props (match WPF TranslateTransform Y="60" + RotateTransform Angle="-4")
        property real entranceSlide: 60
        property real entranceTilt: -4
        transform: [
            Translate { y: panBack.entranceSlide },
            Rotation { angle: panBack.entranceTilt; origin.x: panBack.width / 2; origin.y: panBack.height / 2 }
        ]

        // ---- 8 Resizer handles (exact match FormMain.xaml resizers) ----
        // Top edge
        MouseArea {
            id: resizerT
            anchors { left: parent.left; right: parent.right; top: parent.top; leftMargin: 13; rightMargin: 13 }
            height: 8
            cursorShape: Qt.SizeVerCursor
            onPressed: (mouse) => window.startSystemResize(Qt.TopEdge)
        }
        // Bottom edge
        MouseArea {
            id: resizerB
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom; leftMargin: 13; rightMargin: 13 }
            height: 8
            cursorShape: Qt.SizeVerCursor
            onPressed: (mouse) => window.startSystemResize(Qt.BottomEdge)
        }
        // Left edge
        MouseArea {
            id: resizerL
            anchors { left: parent.left; top: parent.top; bottom: parent.bottom; topMargin: 13; bottomMargin: 13 }
            width: 8
            cursorShape: Qt.SizeHorCursor
            onPressed: (mouse) => window.startSystemResize(Qt.LeftEdge)
        }
        // Right edge
        MouseArea {
            id: resizerR
            anchors { right: parent.right; top: parent.top; bottom: parent.bottom; topMargin: 13; bottomMargin: 13 }
            width: 8
            cursorShape: Qt.SizeHorCursor
            onPressed: (mouse) => window.startSystemResize(Qt.RightEdge)
        }
        // Top-left corner
        MouseArea {
            id: resizerLT
            anchors { left: parent.left; top: parent.top }
            width: 13; height: 13
            cursorShape: Qt.SizeFDiagCursor
            onPressed: (mouse) => window.startSystemResize(Qt.LeftEdge | Qt.TopEdge)
        }
        // Top-right corner
        MouseArea {
            id: resizerRT
            anchors { right: parent.right; top: parent.top }
            width: 13; height: 13
            cursorShape: Qt.SizeBDiagCursor
            onPressed: (mouse) => window.startSystemResize(Qt.RightEdge | Qt.TopEdge)
        }
        // Bottom-left corner
        MouseArea {
            id: resizerLB
            anchors { left: parent.left; bottom: parent.bottom }
            width: 13; height: 13
            cursorShape: Qt.SizeBDiagCursor
            onPressed: (mouse) => window.startSystemResize(Qt.LeftEdge | Qt.BottomEdge)
        }
        // Bottom-right corner
        MouseArea {
            id: resizerRB
            anchors { right: parent.right; bottom: parent.bottom }
            width: 13; height: 13
            cursorShape: Qt.SizeFDiagCursor
            onPressed: (mouse) => window.startSystemResize(Qt.RightEdge | Qt.BottomEdge)
        }

        // ====================================================================
        // BorderForm (Margin=8, Clip RadiusX=6 RadiusY=6)
        // ====================================================================
        Rectangle {
            id: borderForm
            anchors.fill: parent
            anchors.margins: Theme.borderMargin
            radius: Theme.windowCornerRadius
            clip: true
            color: Theme.pureWhite

            // ---- PanForm (Grid with 2 rows: Title + Content) ----
            Item {
                id: panForm
                anchors.fill: parent

                // ---- Background gradient (ImgBack substitute) ----
                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: -0.1; color: Theme.colorBgLeft }
                        GradientStop { position: 0.4; color: Theme.colorBgCenter }
                        GradientStop { position: 1.1; color: Theme.colorBgRight }
                    }
                }

                // ============================================================
                // PanTitle (Height=48) — Title bar
                // ============================================================
                Rectangle {
                    id: panTitle
                    anchors { left: parent.left; right: parent.right; top: parent.top }
                    height: Theme.titleBarHeight
                    z: 10
                    color: Theme.color2

                    // ---- Drag window by title bar (native, no stutter) ----
                    MouseArea {
                        anchors.fill: parent
                        onPressed: (mouse) => window.startSystemMove()
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 13
                        anchors.rightMargin: 8
                        spacing: 0

                        // LPCL logo
                        Text {
                            text: "LPCL"
                            color: "white"
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeLogo
                            font.bold: true
                            Layout.alignment: Qt.AlignVCenter
                            Layout.leftMargin: 6
                        }

                        // Left spacer
                        Item { Layout.fillWidth: true }

                        // Navigation tabs
                        Row {
                            Layout.alignment: Qt.AlignVCenter
                            height: 27
                            spacing: 12

                            Repeater {
                                model: [
                                    { text: "启动", tag: 0, icon: "qrc:/assets/icons/nav_launch.svg" },
                                    { text: "下载", tag: 1, icon: "qrc:/assets/icons/nav_download.svg" },
                                    { text: "设置", tag: 3, icon: "qrc:/assets/icons/nav_settings.svg" },
                                    { text: "更多", tag: 4, icon: "qrc:/assets/icons/nav_more.svg" }
                                ]

                                Item {
                                    width: tabRowContent.width + 20
                                    implicitWidth: width
                                    height: 27
                                    property bool hovered: false

                                    Rectangle {
                                        anchors.fill: parent
                                        radius: tabRowContent.width
                                        color: navTabs.currentIndex === modelData.tag ? "#ffffff" :
                                               parent.hovered ? "#33ffffff" : "transparent"
                                        Behavior on color { ColorAnimation { duration: 100 } }

                                        Row {
                                            id: tabRowContent
                                            anchors.centerIn: parent
                                            height: 27
                                            spacing: 5

                                            // SVG icon from assets (external file, no inline code)
                                            Image {
                                                id: tabIcon
                                                width: 22; height: 22
                                                anchors.verticalCenter: parent.verticalCenter
                                                source: modelData.icon
                                                sourceSize: Qt.size(88, 88)
                                                smooth: true
                                                mipmap: true
                                                visible: false
                                            }
                                            ColorOverlay {
                                                width: 22; height: 22
                                                anchors.verticalCenter: parent.verticalCenter
                                                source: tabIcon
                                                color: navTabs.currentIndex === modelData.tag ? Theme.color2 : "#ffffff"
                                            }

                                            Text {
                                                text: modelData.text
                                                color: navTabs.currentIndex === modelData.tag ? Theme.color2 : "white"
                                                font.family: Theme.fontFamily
                                                font.pixelSize: Theme.fontSize
                                                height: 27
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                        }
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onEntered: parent.hovered = true
                                        onExited: parent.hovered = false
                                        onClicked: { navTabs.currentIndex = modelData.tag }
                                    }
                                }
                            }
                        }

                        // Right spacer
                        Item { Layout.fillWidth: true }

                        // Minimize button
                        Item {
                            id: btnTitleMin
                            Layout.preferredWidth: 28; Layout.preferredHeight: 28
                            Layout.alignment: Qt.AlignVCenter
                            property bool hovered: false

                            Rectangle {
                                anchors.centerIn: parent
                                            
                                            
                                width: 28; height: 28; radius: 3
                                color: btnTitleMin.hovered ? "#33ffffff" : "transparent"
                            }
                            Image {
                                id: imgMin
                                anchors.centerIn: parent
                                sourceSize: Qt.size(96, 96)
                                width: 24; height: 24
                                source: "qrc:/assets/icons/minimize.svg"
                                smooth: true
                                mipmap: true
                                visible: false
                            }
                            ColorOverlay {
                                anchors.fill: imgMin
                                source: imgMin
                                color: "#ffffff"
                            }
                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onEntered: btnTitleMin.hovered = true
                                onExited: btnTitleMin.hovered = false
                                onClicked: window.showMinimized()
                            }
                        }

                        // Close button
                        Item {
                            id: btnTitleClose
                            Layout.preferredWidth: 28; Layout.preferredHeight: 28
                            Layout.alignment: Qt.AlignVCenter
                            property bool hovered: false

                            Rectangle {
                                anchors.centerIn: parent
                                            anchors.leftMargin: 6
                                            anchors.rightMargin: 6
                                width: 28; height: 28; radius: 3
                                color: btnTitleClose.hovered ? "#33ffffff" : "transparent"
                            }
                            Image {
                                anchors.centerIn: parent
                                            anchors.leftMargin: 6
                                            anchors.rightMargin: 6
                                sourceSize: Qt.size(96, 96)
                                width: 24; height: 24
                                id: imgClose
                                source: "qrc:/assets/icons/close.svg"
                                smooth: true
                                mipmap: true
                                visible: false
                            }
                            ColorOverlay {
                                anchors.fill: imgClose
                                source: imgClose
                                color: "#ffffff"
                            }
                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onEntered: btnTitleClose.hovered = true
                                onExited: btnTitleClose.hovered = false
                                onClicked: window.close()
                            }
                        }
                    }

                    // ---- PanTitleInner (sub-page back navigation overlay) ----
                    Item {
                        id: panTitleInner
                        anchors.fill: parent
                        visible: pageStack.length > 0
                        opacity: pageStack.length > 0 ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: 200 } }

                        Item {
                            id: btnTitleInner
                            anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
                            width: 28; height: 28
                            property bool hovered: false

                            Rectangle {
                                anchors.centerIn: parent
                                            anchors.leftMargin: 6
                                            anchors.rightMargin: 6
                                width: 24; height: 24; radius: 12
                                color: btnTitleInner.hovered ? "#33ffffff" : "transparent"
                                Behavior on color { ColorAnimation { duration: 100 } }
                            }
                            Image {
                                anchors.centerIn: parent
                                            anchors.leftMargin: 6
                                            anchors.rightMargin: 6
                                sourceSize: Qt.size(96, 96)
                                width: 24; height: 24
                                id: imgBack
                                source: "qrc:/assets/icons/back.svg"
                                smooth: true
                                mipmap: true
                                visible: false
                            }
                            ColorOverlay {
                                anchors.fill: imgBack
                                source: imgBack
                                color: "#ffffff"
                            }
                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true; cursorShape: Qt.PointingHandCursor
                                onEntered: btnTitleInner.hovered = true
                                onExited: btnTitleInner.hovered = false
                                onClicked: { if (pageStack.length > 0) pageStack.pop() }
                            }
                        }

                        Text {
                            id: labTitleInner
                            text: pageStack.length > 0 ? pageStack[pageStack.length - 1].title : ""
                            color: Theme.color8
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeTitle
                            anchors { left: btnTitleInner.right; leftMargin: 7; verticalCenter: parent.verticalCenter }
                        }
                    }
                }

                // ============================================================
                // Content Area (below title bar)
                // ============================================================
                Item {
                    anchors {
                        left: parent.left
                        right: parent.right
                        top: panTitle.bottom
                        bottom: parent.bottom
                    }

                    RowLayout {
                        anchors.fill: parent
                        spacing: 0

                        // ---- PanLeft (left sidebar) ----
                        Rectangle {
                            id: panLeft
                            Layout.preferredWidth: 300
                            Layout.fillHeight: true
                            color: Theme.sidebarBg

                            // Right shadow edge (RectLeftShadow)
                            Rectangle {
                                anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
                                width: 4
                                opacity: 0.04
                                gradient: Gradient {
                                    GradientStop { position: 0; color: "#000000" }
                                    GradientStop { position: 1; color: "#00000000" }
                                }
                            }

                            // Page content for left sidebar
                            StackLayout {
                                anchors.fill: parent
                                currentIndex: navTabs.currentIndex

                                PageLaunchLeft {}
                                Item { /* Download left — placeholder */ }
                                Item { /* Link left — placeholder */ }
                                Item { /* Settings left — placeholder */ }
                                Item { /* More left — placeholder */ }
                            }
                        }

                        // ---- PanMain (right content area) ----
                        Rectangle {
                            id: panMain
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            color: "transparent"

                            StackLayout {
                                anchors.fill: parent
                                currentIndex: navTabs.currentIndex

                                PageLaunchRight {}
                                Item { /* Download right — placeholder */ }
                                Item { /* Link right — placeholder */ }
                                SettingsPage {}
                                Item { /* More right — placeholder */ }
                            }
                        }
                    }
                }

                // ============================================================
                // PanHint — bottom-left hint overlay
                // ============================================================
                Item {
                    id: panHint
                    anchors { left: parent.left; bottom: parent.bottom; leftMargin: 20; bottomMargin: 20 }
                    // Hint messages appear here
                }

                // ============================================================
                // Bottom-right extra buttons
                // ============================================================
                Column {
                    anchors { right: parent.right; bottom: parent.bottom; rightMargin: 15; bottomMargin: 15 }
                    spacing: 5

                    // BtnExtraBack — Return to top
                    MyIconButton {
                        width: 28; height: 28
                        theme: "Color"
                        logoScale: 0.9
                        iconSource: "qrc:/assets/icons/back_to_top.svg"
                        visible: false  // Shown when scrolled down
                    }
                }

                // ============================================================
                // PanMsg — message overlay (hidden by default)
                // ============================================================
                Rectangle {
                    id: panMsg
                    anchors.fill: parent
                    color: "#00000000"
                    visible: false
                    z: 100
                }
            }
        }
    }

    // ========================================================================
    // Entrance animation (splash is a separate window in main.cpp — see FrmStart)
    // Original FormMain_Loaded:
    //   AaOpacity 250ms | AaDouble Y 60→0 600ms EaseOutBack | AaDouble Angle -4→0 500ms EaseOutBack
    //   All beginTime=100, run in parallel
    // ========================================================================
    Timer {
        id: entranceDelay
        interval: 100  // matches original beginTime=100
        repeat: false
        onTriggered: {
            slideUp.start()
            tiltBack.start()
        }
    }

    NumberAnimation {
        id: fadeInAnim
        target: window; property: "winOpacity"
        to: 1.0
        duration: 250
        easing.type: Easing.OutCubic
    }

    NumberAnimation {
        id: slideUp
        target: panBack; property: "entranceSlide"
        to: 0
        duration: 600
        easing.type: Easing.OutBack
    }

    NumberAnimation {
        id: tiltBack
        target: panBack; property: "entranceTilt"
        to: 0
        duration: 500
        easing.type: Easing.OutBack
    }

    // ========================================================================
    // Navigation state
    // ========================================================================
    property var pageStack: []
    property int currentPage: 0

    QtObject {
        id: navTabs
        property int currentIndex: 0
        onCurrentIndexChanged: {
            window.currentPage = currentIndex
            if (window.pageStack.length === 0 || window.pageStack[window.pageStack.length - 1] !== currentIndex) {
                window.pageStack.push(currentIndex)
            }
        }
    }

    Component.onCompleted: {
        // Entrance animation (splash closes concurrently from main.cpp)
        //   Fade in: opacity 0 → 1.0, 250ms (original AaOpacity)
        //   Slide up: Y 60 → 0, 600ms after 100ms delay (original AaDouble EaseOutBack)
        fadeInAnim.start()
        entranceDelay.start()

        // Initialize — C++ singletons handle startup logic
        JavaManager.scanSystemJava()
        VersionManager.loadLocalVersions()
    }
}
