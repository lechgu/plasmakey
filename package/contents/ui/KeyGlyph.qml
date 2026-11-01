// One key of the indicator. The state is encoded by shape and weight, not
// just color, so it reads in any theme and for color-blind users:
//   off      faint thin outline
//   held     faint fill        (physically pressed)
//   latched  bold outline      (applies to the next key only)
//   locked   solid fill        (stays on until the key is pressed again)
import QtQuick
import org.kde.kirigami as Kirigami
import plasmakey.xkb as Xkb

Item {
    id: key

    property int flags: 0
    property string label: ""
    property bool mapped: true
    property bool showLabel: true
    // Labels only appear when they fit at a readable size (tray icons are
    // usually too small; position and the tooltip identify the keys there).
    property int labelPixelSize: Math.round(height * 0.5)
    readonly property int minLabelPixelSize: 8

    readonly property bool locked: (flags & Xkb.ModifierState.Locked) !== 0
    readonly property bool latched: !locked && (flags & Xkb.ModifierState.Latched) !== 0
    readonly property bool held: !locked && !latched && (flags & Xkb.ModifierState.Depressed) !== 0

    readonly property color ink: Kirigami.Theme.textColor
    readonly property color accent: Kirigami.Theme.highlightColor
    readonly property real side: Math.min(width, height)
    // At least twice the "off" outline so latched never differs by color alone,
    // but thin enough to leave a visible hole (unlike the solid "locked" fill).
    readonly property int boldWidth: Math.max(2, Math.min(Math.round(side * 0.22), Math.floor((side - 2) / 2)))

    function withAlpha(c, a) {
        return Qt.rgba(c.r, c.g, c.b, a);
    }

    opacity: mapped ? 1 : 0.4

    Rectangle {
        anchors.fill: parent
        radius: Math.max(1, key.side * 0.2)
        antialiasing: true
        color: key.locked ? key.accent : key.held ? key.withAlpha(key.ink, 0.35) : "transparent"
        border.width: key.latched ? key.boldWidth : 1
        border.color: (key.locked || key.latched) ? key.accent : key.withAlpha(key.ink, key.held ? 0.6 : 0.45)
    }

    Text {
        id: labelText
        anchors.fill: parent
        anchors.margins: key.latched ? key.boldWidth + 1 : 2
        visible: key.showLabel && key.label.length > 0 && key.labelPixelSize >= key.minLabelPixelSize
            && contentWidth <= width + 0.5 && contentHeight <= height + 0.5
        text: key.label
        color: key.locked ? Kirigami.Theme.highlightedTextColor : key.ink
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        fontSizeMode: Text.Fit
        minimumPixelSize: key.minLabelPixelSize
        font.pixelSize: key.labelPixelSize
        font.family: Kirigami.Theme.defaultFont.family
    }
}
