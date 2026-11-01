// Popup shown when the tray icon is clicked: state in words plus a legend.
// Not needed to read the state; the tray icon alone shows it.
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.components as PlasmaComponents
import plasmakey.xkb as Xkb

Item {
    id: full

    required property var plasmoidItem
    required property var xkb

    readonly property bool working: xkb.supported && xkb.connected

    Layout.minimumWidth: Kirigami.Units.gridUnit * 14
    Layout.preferredWidth: Kirigami.Units.gridUnit * 18
    Layout.preferredHeight: content.implicitHeight + Kirigami.Units.largeSpacing * 2
    Layout.minimumHeight: Layout.preferredHeight

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing

        PlasmaComponents.Label {
            Layout.fillWidth: true
            visible: !full.working
            text: full.xkb.problem
            wrapMode: Text.Wrap
        }

        RowLayout {
            visible: full.working
            spacing: Kirigami.Units.largeSpacing

            KeyboardGlyph {
                size: Kirigami.Units.gridUnit * 6
                shiftFlags: full.xkb.shift
                controlFlags: full.xkb.control
                metaFlags: full.xkb.meta
                altFlags: full.xkb.alt
                metaMapped: full.xkb.metaMapped
                altMapped: full.xkb.altMapped
            }

            GridLayout {
                columns: 2
                columnSpacing: Kirigami.Units.largeSpacing
                rowSpacing: Kirigami.Units.smallSpacing

                Repeater {
                    model: full.plasmoidItem.modifiers
                    delegate: PlasmaComponents.Label {
                        required property var modelData
                        required property int index
                        Layout.row: index
                        Layout.column: 0
                        text: modelData.name
                        font.bold: modelData.flags !== 0
                    }
                }
                Repeater {
                    model: full.plasmoidItem.modifiers
                    delegate: PlasmaComponents.Label {
                        required property var modelData
                        required property int index
                        Layout.row: index
                        Layout.column: 1
                        text: modelData.mapped ? full.plasmoidItem.stateName(modelData.flags) : i18nc("modifier has no key", "not mapped")
                        font.bold: modelData.flags !== 0
                        opacity: modelData.flags !== 0 ? 1 : 0.7
                    }
                }
            }
        }

        Kirigami.Separator {
            Layout.fillWidth: true
        }

        // Legend: the same key drawing in each state.
        GridLayout {
            columns: 2
            columnSpacing: Kirigami.Units.largeSpacing
            rowSpacing: Kirigami.Units.smallSpacing

            Repeater {
                model: [
                    { flags: Xkb.ModifierState.Inactive, text: i18n("Off") },
                    { flags: Xkb.ModifierState.Depressed, text: i18n("Held down") },
                    { flags: Xkb.ModifierState.Latched, text: i18n("Latched: applies to the next key only") },
                    { flags: Xkb.ModifierState.Locked, text: i18n("Locked: stays on until you press the key again") }
                ]
                delegate: RowLayout {
                    required property var modelData
                    Layout.columnSpan: 2
                    spacing: Kirigami.Units.largeSpacing

                    KeyGlyph {
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 1.6
                        Layout.preferredHeight: Kirigami.Units.gridUnit * 1.2
                        flags: modelData.flags
                        showLabel: false
                    }
                    PlasmaComponents.Label {
                        Layout.fillWidth: true
                        text: modelData.text
                        wrapMode: Text.Wrap
                    }
                }
            }
        }

        Kirigami.Separator {
            Layout.fillWidth: true
            visible: full.working && full.xkb.stickyKeysKnown
        }

        PlasmaComponents.Label {
            Layout.fillWidth: true
            visible: full.working && full.xkb.stickyKeysKnown
            wrapMode: Text.Wrap
            opacity: 0.7
            text: !full.xkb.stickyKeysEnabled ? i18n("Sticky Keys is turned off in System Settings → Accessibility.")
                : full.xkb.stickyKeysLockEnabled ? i18n("Sticky Keys is on. Press a modifier once to latch it, twice to lock it.")
                : i18n("Sticky Keys is on. Press a modifier once to latch it (locking is turned off).")
        }
    }
}
