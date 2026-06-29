import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import PCL.Core
import "src/ui/components"
import "src/ui/pages"
import "src/ui/styles"

ApplicationWindow {
    id: window

    // Window properties matching original
    width: Theme.windowWidth
    height: Theme.windowHeight
    minimumWidth: Theme.windowMinWidth
    minimumHeight: Theme.windowMinHeight
    visible: true
    title: "Plain Craft Launcher  "

    // Frameless + transparent for custom chrome (like original WindowStyle="None" AllowsTransparency="True")
    flags: Qt.FramelessWindowHint | Qt.Window | Qt.WindowStaysOnTopHint
    color: "transparent"

    // ====================================================================
    // Root container with tilt effect (-4 degrees + 60px Y offset)
    // ====================================================================
    Item {
        id: panBack
        anchors.fill: parent
        anchors.margins: Theme.windowMargin

        // Tilt transform: -4 degrees rotation + 60px Y offset
        transform: [
            Rotation {
                origin.x: panBack.width / 2
                origin.y: 0
                angle: -4
            },
            Translate {
                y: 60
            }
        ]

        // Resizer handles (8 edges + corners) — simplified for Qt
        MouseArea {
            id: resizeLeft
            anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
            width: 8
            cursorShape: Qt.SizeHorCursor
            onMouseXChanged: { window.width = Math.max(window.minimumWidth, window.width - mouseX) }
        }
        MouseArea {
            id: resizeRight
            anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
            width: 8
            cursorShape: Qt.SizeHorCursor
            onMouseXChanged: { window.width = Math.max(window.minimumWidth, window.width + mouseX) }
        }
        MouseArea {
            id: resizeBottom
            anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
            height: 8
            cursorShape: Qt.SizeVerCursor
            onMouseYChanged: { window.height = Math.max(window.minimumHeight, window.height + mouseY) }
        }

        // Visible UI area (BorderForm)
        Rectangle {
            id: borderForm
            anchors.fill: parent
            anchors.margins: Theme.borderMargin
            radius: Theme.windowCornerRadius
            clip: true
            color: Theme.pureWhite

            // ============================================================
            // Title Bar (PanTitle, Height=48)
            // ============================================================
            Rectangle {
                id: panTitle
                anchors { left: parent.left; right: parent.right; top: parent.top }
                height: Theme.titleBarHeight
                z: 10

                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.titleGradTop }
                    GradientStop { position: 0.5; color: Theme.titleGradMid }
                    GradientStop { position: 1.0; color: Theme.titleGradBot }
                }

                // Drag window by title bar
                MouseArea {
                    anchors.fill: parent
                    property point lastPos: Qt.point(0, 0)
                    onPressed: { lastPos = Qt.point(mouse.x, mouse.y) }
                    onPositionChanged: {
                        if (pressed) {
                            window.x += mouse.x - lastPos.x
                            window.y += mouse.y - lastPos.y
                        }
                    }
                }

                Row {
                    anchors { right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
                    spacing: 4

                    // Minimize button
                    Rectangle {
                        width: 28; height: 28; radius: 3
                        color: minimizeArea.containsMouse ? "#33ffffff" : "transparent"
                        Canvas {
                            anchors.centerIn: parent
                            width: 12; height: 2
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.fillStyle = "white"
                                ctx.fillRect(0, 0, width, height)
                            }
                        }
                        MouseArea {
                            id: minimizeArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: window.showMinimized()
                        }
                    }

                    // Close button
                    Rectangle {
                        width: 28; height: 28; radius: 3
                        color: closeArea.containsMouse ? "#E81123" : "transparent"
                        Canvas {
                            anchors.centerIn: parent
                            width: 12; height: 12
                            onPaint: {
                                var ctx = getContext("2d")
                                ctx.strokeStyle = "white"
                                ctx.lineWidth = 1.8
                                ctx.beginPath()
                                ctx.moveTo(2, 2); ctx.lineTo(10, 10)
                                ctx.moveTo(10, 2); ctx.lineTo(2, 10)
                                ctx.stroke()
                            }
                        }
                        MouseArea {
                            id: closeArea
                            anchors.fill: parent
                            hoverEnabled: true
                            onClicked: window.close()
                        }
                    }
                }

                // Navigation tabs (PanTitleSelect)
                Row {
                    id: panTitleSelect
                    anchors { left: parent.left; leftMargin: 13; verticalCenter: parent.verticalCenter }
                    height: 27
                    spacing: 2

                    // Navigation buttons — matching original 5 tabs
                    Repeater {
                        model: [
                            { text: "Launch", tag: 0, icon: "M4,4L12,12L20,4" },
                            { text: "Download", tag: 1, icon: "M4,16L12,8L20,16" },
                            { text: "Link", tag: 2, icon: "M12,2C6.48,2 2,6.48 2,12s4.48,10 10,10 10-4.48 10-10S17.52,2 12,2z" },
                            { text: "Settings", tag: 3, icon: "M12,15.5A3.5,3.5 0 0,1 8.5,12 3.5,3.5 0 0,1 12,8.5a3.5,3.5 0 0,1 3.5,3.5 3.5,3.5 0 0,1 -3.5,3.5zM19.43,12.97c0.04,-0.32 0.07,-0.64 0.07,-0.97 0,-0.33 -0.03,-0.66 -0.07,-0.98l2.11,-1.65c0.19,-0.15 0.24,-0.42 0.12,-0.64l-2,-3.46c-0.12,-0.22 -0.39,-0.3 -0.61,-0.22l-2.49,1c-0.52,-0.4 -1.08,-0.73 -1.69,-0.98l-0.38,-2.65C14.46,2.18 14.25,2 14,2h-4c-0.25,0 -0.46,0.18 -0.49,0.42l-0.38,2.65c-0.61,0.25 -1.17,0.59 -1.69,0.98l-2.49,-1c-0.23,-0.09 -0.49,0 -0.61,0.22l-2,3.46c-0.13,0.22 -0.07,0.49 0.12,0.64l2.11,1.65c-0.04,0.32 -0.07,0.65 -0.07,0.98 0,0.33 0.03,0.66 0.07,0.97l-2.11,1.65c-0.19,0.15 -0.24,0.42 -0.12,0.64l2,3.46c0.12,0.22 0.39,0.3 0.61,0.22l2.49,-1c0.52,0.4 1.08,0.73 1.69,0.98l0.38,2.65c0.03,0.24 0.24,0.42 0.49,0.42h4c0.25,0 0.46,-0.18 0.49,-0.42l0.38,-2.65c0.61,-0.25 1.17,-0.59 1.69,-0.98l2.49,1c0.23,0.09 0.49,0 0.61,-0.22l2,-3.46c0.12,-0.22 0.07,-0.49 -0.12,-0.64l-2.11,-1.65z" },
                            { text: "More", tag: 4, icon: "M6,10c-1.1,0 -2,0.9 -2,2s0.9,2 2,2 2,-0.9 2,-2 -0.9,-2 -2,-2zM12,10c-1.1,0 -2,0.9 -2,2s0.9,2 2,2 2,-0.9 2,-2 -0.9,-2 -2,-2zM18,10c-1.1,0 -2,0.9 -2,2s0.9,2 2,2 2,-0.9 2,-2 -0.9,-2 -2,-2z" }
                        ]

                        Rectangle {
                            width: tabLabel.implicitWidth + 20
                            height: 27
                            radius: 3
                            color: navTabs.currentIndex === modelData.tag ? "#33ffffff" : "transparent"

                            property bool hovered: false
                            Behavior on color { ColorAnimation { duration: 100 } }

                            Text {
                                id: tabLabel
                                anchors.centerIn: parent
                                text: modelData.text
                                color: "white"
                                font.family: Theme.fontFamily
                                font.pixelSize: Theme.fontSize
                            }

                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                onEntered: parent.hovered = true
                                onExited: parent.hovered = false
                                onClicked: {
                                    navTabs.currentIndex = modelData.tag
                                }
                            }
                        }
                    }
                }

                // Sub-page navigation (PanTitleInner) — initially hidden
                Rectangle {
                    id: panTitleInner
                    anchors.fill: parent
                    color: "transparent"
                    opacity: 0
                    visible: false

                    Row {
                        anchors { left: parent.left; leftMargin: 8; verticalCenter: parent.verticalCenter }
                        spacing: 8

                        // Back button
                        Rectangle {
                            width: 28; height: 28; radius: 3
                            color: backHover.containsMouse ? "#33ffffff" : "transparent"
                            Canvas {
                                anchors.centerIn: parent
                                width: 14; height: 14
                                onPaint: {
                                    var ctx = getContext("2d")
                                    ctx.strokeStyle = "white"
                                    ctx.lineWidth = 1.8
                                    ctx.beginPath()
                                    ctx.moveTo(10, 2); ctx.lineTo(4, 7); ctx.lineTo(10, 12)
                                    ctx.stroke()
                                }
                            }
                            MouseArea {
                                id: backHover
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: pageStack.pop()
                            }
                        }

                        Text {
                            id: labTitleInner
                            text: ""
                            color: Theme.color8
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontSizeTitle
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }

            // ============================================================
            // Content Area
            // ============================================================
            RowLayout {
                anchors {
                    left: parent.left
                    right: parent.right
                    top: panTitle.bottom
                    bottom: parent.bottom
                }
                spacing: 0

                // Left sidebar (PanLeft)
                Rectangle {
                    id: panLeft
                    Layout.preferredWidth: 300
                    Layout.minimumWidth: 250
                    Layout.maximumWidth: 300
                    Layout.fillHeight: true
                    color: Theme.sidebarBg

                    // Right shadow edge
                    Rectangle {
                        anchors { right: parent.right; top: parent.top; bottom: parent.bottom }
                        width: 4
                        gradient: Gradient {
                            GradientStop { position: 0; color: "#0a000000" }
                            GradientStop { position: 1; color: "transparent" }
                        }
                    }

                    // Page content for left sidebar
                    StackLayout {
                        id: leftPageStack
                        anchors.fill: parent
                        currentIndex: navTabs.currentIndex

                        PageLaunchLeft {}
                        Item { /* Download left */ }
                        Item { /* Link left */ }
                        Item { /* Settings left */ }
                        Item { /* More left */ }
                    }
                }

                // Right content area (PanMain)
                Rectangle {
                    id: panMain
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: Theme.pureWhite

                    StackLayout {
                        id: mainPageStack
                        anchors.fill: parent
                        currentIndex: navTabs.currentIndex

                        PageLaunchRight {}
                        LoginPage {}     // Download tab
                        Item {}          // Link tab
                        SettingsPage {}  // Settings tab
                        Item {}          // More tab
                    }
                }
            }

            // Hint area (bottom left overlay)
            // Extra buttons area (bottom right overlay)
        }
    }

    // Page stack for back-navigation
    property var pageStack: []

    // Navigation tab controller
    property int currentPage: 0

    QtObject {
        id: navTabs
        property int currentIndex: 0
        onCurrentIndexChanged: {
            window.currentPage = currentIndex
            // Add to page stack for back navigation
            if (window.pageStack.length === 0 || window.pageStack[window.pageStack.length - 1] !== currentIndex) {
                window.pageStack.push(currentIndex)
            }
        }
    }

    Component.onCompleted: {
        // Initialize Java scan
        JavaManager.scanSystemJava()
        VersionManager.loadLocalVersions()
    }
}
