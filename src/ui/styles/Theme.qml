pragma Singleton
import QtQuick

QtObject {
    // Colors (dark theme as default, matching original PCL)
    property color bgPrimary: "#1a1a2e"
    property color bgSecondary: "#16213e"
    property color bgCard: "#1f2b47"
    property color bgInput: "#0f3460"
    property color accent: "#e94560"
    property color accentHover: "#ff6b6b"
    property color textPrimary: "#eaeaea"
    property color textSecondary: "#a0a0b0"
    property color textDisabled: "#666680"
    property color success: "#4ecca3"
    property color warning: "#f0a500"
    property color error: "#e94560"
    property color border: "#2a3a5c"

    // Light theme
    property color lightBgPrimary: "#f0f2f5"
    property color lightBgSecondary: "#ffffff"
    property color lightBgCard: "#ffffff"
    property color lightBgInput: "#f0f2f5"
    property color lightAccent: "#1890ff"
    property color lightTextPrimary: "#1a1a2e"
    property color lightTextSecondary: "#666680"
    property color lightBorder: "#d9d9d9"

    // Spacing
    property int spacingXs: 4
    property int spacingSm: 8
    property int spacingMd: 16
    property int spacingLg: 24
    property int spacingXl: 32

    // Radius
    property int radiusSm: 4
    property int radiusMd: 8
    property int radiusLg: 12
    property int radiusXl: 16

    // Font sizes
    property int fontSizeXs: 11
    property int fontSizeSm: 13
    property int fontSizeMd: 15
    property int fontSizeLg: 18
    property int fontSizeXl: 24
    property int fontSizeXxl: 32
}
