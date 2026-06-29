pragma Singleton
import QtQuick

QtObject {
    // ========================================================================
    // HSL-based dynamic theme (mirrors ModSecret.ThemeRefresh)
    // Default: Hue=210, Sat=85, LightAdjust=0
    // ========================================================================
    property real hue: 210
    property real sat: 85
    property real lightAdj: 0

    // Derived HSL -> color helper
    function hsl(h, s, l) {
        return Qt.hsla(h / 360, s / 100, l / 100, 1.0)
    }

    // Color 1-8 (documented defaults)
    readonly property color color1: "#343d4a"   // Dark text
    readonly property color color2: "#0b5bcb"   // Highlight border
    readonly property color color3: "#1370f3"   // Focus border
    readonly property color color4: "#4890f5"   // Title bar / hover border
    readonly property color color5: "#96c0f9"   // Light tint
    readonly property color color6: "#d5e6fd"   // Very light bg
    readonly property color color7: "#e0eafd"   // Hover bg
    readonly property color color8: "#eaf2fe"   // Sub-page title

    // Derived
    readonly property color colorBg0: "#96c0f9"  // color4*0.4 + color5*0.4 + gray4*0.2

    // Gray scale
    readonly property color gray1: "#404040"
    readonly property color gray2: "#737373"
    readonly property color gray3: "#8c8c8c"
    readonly property color gray4: "#a6a6a6"
    readonly property color gray5: "#cccccc"
    readonly property color gray6: "#ebebeb"
    readonly property color gray7: "#f0f0f0"
    readonly property color gray8: "#f5f5f5"

    // Special brushes
    readonly property color halfWhite: "#55ffffff"      // Semi-transparent white
    readonly property color semiWhite: "#bbffffff"       // Nearly opaque white
    readonly property color pureWhite: "#ffffff"         // Pure white
    readonly property color semiTransparent: "#01eaf2fe" // Near-transparent light blue (thumb track)
    readonly property color sidebarBg: "#f1ffffff"       // 94.5% opaque white
    readonly property color redBack: "#80fbdddd"         // 50% alpha pink
    readonly property color redLight: "#ff4c4c"          // Red highlight
    readonly property color redDark: "#ce2111"           // Red dark

    // Title bar gradient stops
    readonly property color titleGradTop: "#5699f6"     // ~hsl(210,85,54)
    readonly property color titleGradMid: "#4890f5"     // ~hsl(210,85,48)
    readonly property color titleGradBot: "#3b82f2"     // ~hsl(210,85,45)

    // ========================================================================
    // Font
    // ========================================================================
    property string fontFamily: "Microsoft YaHei UI, PingFang SC, Noto Sans CJK SC, sans-serif"
    readonly property int fontSize: 13
    readonly property int fontSizeSmall: 12
    readonly property int fontSizeXSmall: 11
    readonly property int fontSizeLarge: 14
    readonly property int fontSizeTitle: 15
    readonly property int fontSizeLogo: 17
    readonly property int fontSizeLaunchTitle: 20
    readonly property real fontSizeLaunchLabel: 12.5
    readonly property real fontSizeLaunchName: 13.5

    // ========================================================================
    // Dimensions
    // ========================================================================
    readonly property int windowWidth: 850
    readonly property int windowHeight: 500
    readonly property int windowMinWidth: 810
    readonly property int windowMinHeight: 470
    readonly property int windowCornerRadius: 6
    readonly property int windowMargin: 10       // PanBack margin
    readonly property int borderMargin: 8        // BorderForm margin
    readonly property int titleBarHeight: 48

    // Control dimensions
    readonly property int buttonRadius: 3
    readonly property int inputRadius: 3
    readonly property int tooltipRadius: 4
    readonly property int menuRadius: 3
    readonly property int scrollbarWidth: 8
    readonly property int scrollbarThumbRadius: 3
    readonly property int launchBtnHeight: 54
    readonly property int versionBtnHeight: 35
    readonly property int folderItemHeight: 40
    readonly property int actionItemHeight: 34
    readonly property int progressBarHeight: 4

    // ========================================================================
    // Animations
    // ========================================================================
    // Duration helpers
    function easeOutBack(t) { var c1=1.70158; var c3=c1+1; return 1+c3*Math.pow(t-1,3)+c1*Math.pow(t-1,2) }
    function easeOutFluent(t) { return 1-Math.pow(1-t, 1.8) }
    function easeInFluent(t) { return Math.pow(t, 1.8) }
}
