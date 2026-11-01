// Four keys laid out like the bottom-left corner of a keyboard:
//   [      Shift      ]
//   [Ctrl] [Meta] [Alt]
import QtQuick

Item {
    id: glyph

    property real size: 22
    property int shiftFlags: 0
    property int controlFlags: 0
    property int metaFlags: 0
    property int altFlags: 0
    property bool metaMapped: true
    property bool altMapped: true

    // Whole pixels keep the thin outlines crisp.
    readonly property int pad: Math.max(1, Math.round(size * 0.05))
    readonly property int gap: Math.max(1, Math.round(size * 0.06))
    readonly property int keyWidth: Math.floor((size - 2 * pad - 2 * gap) / 3)
    readonly property int rowWidth: 3 * keyWidth + 2 * gap
    readonly property int keyHeight: Math.floor((size - 2 * pad - gap) / 2)
    readonly property int originX: Math.round((size - rowWidth) / 2)
    readonly property int originY: Math.round((size - (2 * keyHeight + gap)) / 2)
    // One size for all labels, small enough for the widest ("Meta") to fit.
    readonly property int labelPixelSize: Math.floor(Math.min(keyHeight * 0.45, keyWidth / 2.6))

    width: size
    height: size

    KeyGlyph {
        x: glyph.originX
        y: glyph.originY
        width: glyph.rowWidth
        height: glyph.keyHeight
        labelPixelSize: Math.floor(glyph.keyHeight * 0.5) // one glyph on a wide key: can be larger
        flags: glyph.shiftFlags
        label: "⇧"
    }

    Row {
        x: glyph.originX
        y: glyph.originY + glyph.keyHeight + glyph.gap
        spacing: glyph.gap

        KeyGlyph {
            width: glyph.keyWidth
            height: glyph.keyHeight
            labelPixelSize: glyph.labelPixelSize
            flags: glyph.controlFlags
            label: "Ctrl"
        }
        KeyGlyph {
            width: glyph.keyWidth
            height: glyph.keyHeight
            labelPixelSize: glyph.labelPixelSize
            flags: glyph.metaFlags
            mapped: glyph.metaMapped
            label: "Meta"
        }
        KeyGlyph {
            width: glyph.keyWidth
            height: glyph.keyHeight
            labelPixelSize: glyph.labelPixelSize
            flags: glyph.altFlags
            mapped: glyph.altMapped
            label: "Alt"
        }
    }
}
