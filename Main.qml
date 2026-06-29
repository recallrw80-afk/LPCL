import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PCL.Core
import "src/ui/components"
import "src/ui/pages"
import "src/ui/styles"

ApplicationWindow {
    id: window

    // ---- Window properties (exact match FormMain.xaml) ----
    width: Theme.windowWidth
    height: Theme.windowHeight
    minimumWidth: Theme.windowMinWidth
    minimumHeight: Theme.windowMinHeight
    visible: true
    title: "Plain Craft Launcher  "

    // Frameless + transparent for custom chrome (WindowStyle="None" AllowsTransparency="True" Topmost="True")
    flags: Qt.FramelessWindowHint | Qt.Window | Qt.WindowStaysOnTopHint
    color: "transparent"

    // ========================================================================
    // PanBack (Grid, Margin=10)
    // ========================================================================
    Item {
        id: panBack
        anchors.fill: parent
        anchors.margins: Theme.windowMargin

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

                    // Horizontal gradient: darker edges → lighter center → darker edges
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: Theme.titleGradEdge }
                        GradientStop { position: 0.5; color: Theme.titleGradCenter }
                        GradientStop { position: 1.0; color: Theme.titleGradEnd }
                    }

                    // ---- Drag window by title bar ----
                    MouseArea {
                        anchors.fill: parent
                        property point lastPos: Qt.point(0, 0)
                        onPressed: (mouse) => { lastPos = Qt.point(mouse.x, mouse.y) }
                        onPositionChanged: (mouse) => {
                            if (pressed) {
                                window.x += mouse.x - lastPos.x
                                window.y += mouse.y - lastPos.y
                            }
                        }
                    }

                    // ---- Close button (MyIconButton, White, X icon) ----
                    MyIconButton {
                        id: btnTitleClose
                        anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                        width: 28; height: 28
                        theme: "White"
                        logoScale: 0.72
                        logo: "F1 M2,0 L0,2 8,10 0,18 2,20 10,12 18,20 20,18 12,10 20,2 18,0 10,8 2,0Z"
                        onClicked: window.close()
                    }

                    // ---- Minimize button (MyIconButton, White, - icon) ----
                    MyIconButton {
                        id: btnTitleMin
                        anchors { right: btnTitleClose.left; rightMargin: 4; verticalCenter: parent.verticalCenter }
                        width: 28; height: 28
                        theme: "White"
                        logoScale: 0.72
                        logo: "F1 M0,0 h15 v2 h-15 v-2 Z"
                        onClicked: window.showMinimized()
                    }

                    // ---- PanTitleMain (navigation tabs) ----
                    Row {
                        id: panTitleSelect
                        anchors { left: parent.left; leftMargin: 13; verticalCenter: parent.verticalCenter }
                        height: 27
                        spacing: 2

                        // Navigation tab buttons (exact match original BtnTitleSelect0-4)
                        Repeater {
                            model: [
                                {
                                    text: "启动", tag: 0,
                                    logo: "M52.1,164.5c-1.4,0-3.1-0.5-4.2-1.3c-2.6-1.7-4-4.2-4-7V43.8c0-2.9,1.6-5.8,4.1-7c1.2-0.8,2.7-1.2,4.1-1.2c1.5,0,2.9,0.4,4.2,1.2L153.1,93c0,0,0.1,0,0.1,0.1c2.6,1.7,4,4.2,4,7c0,3-1.7,5.8-4.2,7.1l-96.8,56.2C55.1,164,53.5,164.5,52.1,164.5z"
                                },
                                {
                                    text: "下载", tag: 1,
                                    logo: "M955 610h-59c-15 0-29 13-29 29v196c0 15-13 29-29 29h-649c-15 0-29-13-29-29v-196c0-15-13-29-29-29h-59c-15 0-29 13-29 29V905c0 43 35 78 78 78h787c43 0 78-35 78-78V640c0-15-13-29-29-29zM492 740c11 11 29 11 41 0l265-265c11-11 11-29 0-41l-41-41c-11-11-29-11-41 0l-110 110c-11 11-33 3-33-13V68C571 53 555 39 541 39h-59c-15 0-29 13-29 29v417c0 17-21 25-33 13l-110-110c-11-11-29-11-41 0L226 433c-11 11-11 29 0 41L492 740z"
                                },
                                {
                                    text: "联机", tag: 2,
                                    logo: "M512 817c-48.601 0-88-39.399-88-88s39.399-88 88-88 88 39.399 88 88-39.399 88-88 88zM237.671 565.74C308.335 474.58 397.369 429 504.774 429c118.433 0 214.225 55.421 287.377 166.264l-53.407 30.369c-13.84 7.87-31.362 4.367-41.114-8.219-50.291-64.911-114.577-97.367-192.856-97.367-86.851 0-156.835 40.318-209.95 120.953l-47.995-28.02c-15.263-8.91-20.412-28.507-11.502-43.77a32 32 0 0 1 2.344-3.47zM107.691 419.47C205.24 278.491 337.805 208 505.379 208c178.77 0 317.694 80.224 416.772 240.672l-56.54 31.73c-13.686 7.68-30.922 4.303-40.697-7.975C735.581 360.213 629.07 303.009 505.38 300.815 373.997 298.485 261.637 362.88 168.3 494l-50.116-28.505c-15.362-8.738-20.732-28.275-11.994-43.637a32 32 0 0 1 1.5-2.387z"
                                },
                                {
                                    text: "设置", tag: 3,
                                    logo: "M940.4 463.7L773.3 174.2c-17.3-30-49.2-48.4-83.8-48.4H340.2c-34.6 0-66.5 18.5-83.8 48.4L89.2 463.7c-17.3 30-17.3 66.9 0 96.8L256.4 850c17.3 30 49.2 48.4 83.8 48.4h349.2c34.6 0 66.5-18.5 83.8-48.4l167.2-289.5c17.3-29.9 17.3-66.8 0-96.8z"
                                },
                                {
                                    text: "更多", tag: 4,
                                    logo: "M364 0h-273C40 0 0 40 0 91v273C0 414 40 455 91 455h273C414 455 455 414 455 364V91C455 40 414 0 364 0zM341 341H113V113h227v227zM933 0h-273C609 0 568 40 568 91v273c0 50 40 91 91 91h273C983 455 1024 414 1024 364V91c0-50-40-90-90-90zM910 341h-227V113h227v227zM364 568h-273C40 568 0 609 0 659v273c0 50 40 91 91 91h273C414 1024 455 983 455 932v-273C455 609 414 568 364 568zM341 910H113v-227h227v227zM933 568h-273c-50 0-91 40-91 91v273c0 50 40 91 91 91h273c50 0 90-40 90-91v-273c0-50-40-90-90-90zM910 910h-227v-227h227v227z"
                                }
                            ]

                            Item {
                                width: labelRow.implicitWidth + 20
                                height: 27

                                property bool hovered: false

                                Rectangle {
                                    anchors.fill: parent
                                    radius: 3
                                    color: {
                                        if (navTabs.currentIndex === modelData.tag) return "#33ffffff"
                                        if (parent.hovered) return "#18ffffff"
                                        return "transparent"
                                    }
                                    Behavior on color { ColorAnimation { duration: 100 } }

                                    Row {
                                        id: labelRow
                                        anchors.centerIn: parent
                                        spacing: 5

                                        // Icon (simplified: small colored dot for now, MyIconButton would need fitting)
                                        Rectangle {
                                            width: 10; height: 10
                                            anchors.verticalCenter: parent.verticalCenter
                                            radius: 5
                                            color: navTabs.currentIndex === modelData.tag ? "white" : "#88ffffff"
                                            visible: false   // Hidden for now — SVG icons will go here
                                        }

                                        Text {
                                            text: modelData.text
                                            color: "white"
                                            font.family: Theme.fontFamily
                                            font.pixelSize: Theme.fontSize
                                            anchors.verticalCenter: parent.verticalCenter
                                        }
                                    }
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onEntered: parent.hovered = true
                                    onExited: parent.hovered = false
                                    onClicked: {
                                        navTabs.currentIndex = modelData.tag
                                    }
                                }
                            }
                        }
                    }

                    // ---- PanTitleInner (sub-page back navigation, hidden by default) ----
                    // Shown when navigating into sub-pages
                    Item {
                        id: panTitleInner
                        anchors.fill: parent
                        visible: pageStack.length > 0
                        opacity: pageStack.length > 0 ? 1 : 0
                        Behavior on opacity { NumberAnimation { duration: 200 } }

                        MyIconButton {
                            id: btnTitleInner
                            anchors { left: parent.left; leftMargin: 12; verticalCenter: parent.verticalCenter }
                            width: 28; height: 28
                            theme: "White"
                            logoScale: 0.87
                            logo: "M1097 584 250 584 562 896C591 925 591 972 562 1001 533 1030 487 1030 458 1001L21 565C6 550-0 531 0 511L0 511 0 511C-0 492 6 472 21 457L458 21C487-7 533-7 562 21 591 50 591 97 562 126L250 438 1097 438C1137 438 1170 471 1170 511 1170 551 1137 584 1097 584L1097 584Z"
                            onClicked: {
                                if (pageStack.length > 0) pageStack.pop()
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
                        logo: "F1 M858.496 188.9024 173.1072 188.9024c-30.2848 0-54.8352-24.5504-54.8352-54.8352L118.272 106.6496c0-30.2848 24.5504-54.8352 54.8352-54.8352l685.3888 0c30.2848 0 54.8352 24.5504 54.8352 54.8352l0 27.4176C913.3312 164.352 888.7808 188.9024 858.496 188.9024z"
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
        // Initialize — C++ singletons handle startup logic
        JavaManager.scanSystemJava()
        VersionManager.loadLocalVersions()
    }
}
