// Offscreen gallery of the plasmoid's real QML files with fake XKB state.
// Rendered by plasmakey-preview; `make preview` writes build/preview.png.
import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami
import plasmakey.xkb as Xkb
import "../../package/contents/ui" as UI

Window {
    id: win
    visible: true
    width: gallery.implicitWidth
    height: gallery.implicitHeight
    color: "white"

    readonly property int off: Xkb.ModifierState.Inactive
    readonly property int held: Xkb.ModifierState.Depressed
    readonly property int latched: Xkb.ModifierState.Latched
    readonly property int locked: Xkb.ModifierState.Locked

    // Same shape as ModifierState, settable from here.
    component FakeState: QtObject {
        property bool supported: true
        property bool connected: true
        property string problem: ""
        property int shift: 0
        property int control: 0
        property int alt: 0
        property int meta: 0
        property bool altMapped: true
        property bool metaMapped: true
        property bool stickyKeysKnown: true
        property bool stickyKeysEnabled: true
        property bool stickyKeysLockEnabled: true
    }

    // Same shape as main.qml's PlasmoidItem, as far as the representations use it.
    component FakePlasmoid: QtObject {
        required property var xkb
        property bool expanded: false
        property string toolTipMainText: "plasmakey"
        property string toolTipSubText: ""
        readonly property var modifiers: [
            { name: "Shift", flags: xkb.shift, mapped: true },
            { name: "Ctrl", flags: xkb.control, mapped: true },
            { name: "Meta", flags: xkb.meta, mapped: xkb.metaMapped },
            { name: "Alt", flags: xkb.alt, mapped: xkb.altMapped }
        ]
        function stateName(flags) {
            if (flags & Xkb.ModifierState.Locked) return "locked";
            if (flags & Xkb.ModifierState.Latched) return "latched";
            if (flags & Xkb.ModifierState.Depressed) return "held";
            return "off";
        }
    }

    readonly property var cases: [
        { title: "idle", s: {} },
        { title: "Shift held", s: { shift: held } },
        { title: "Ctrl latched", s: { control: latched } },
        { title: "Ctrl locked", s: { control: locked } },
        { title: "Shift latched,\nCtrl locked, Meta held", s: { shift: latched, control: locked, meta: held } },
        { title: "all locked", s: { shift: locked, control: locked, meta: locked, alt: locked } },
        { title: "Sticky Keys off", s: { stickyKeysEnabled: false } },
        { title: "Wayland", s: { supported: false, problem: "Not supported in this session (wayland)." } }
    ]

    function makeState(props) {
        return fakeState.createObject(win, props);
    }
    Component { id: fakeState; FakeState {} }
    Component { id: fakePlasmoid; FakePlasmoid {} }

    // One theme column: the tray icon at real size, zoomed, and at larger sizes.
    component ThemePanel: Rectangle {
        id: panel
        property string themeName
        implicitWidth: col.implicitWidth + 24
        implicitHeight: col.implicitHeight + 24
        color: Kirigami.Theme.backgroundColor

        ColumnLayout {
            id: col
            x: 12
            y: 12
            spacing: 10

            Text {
                text: panel.themeName
                font.bold: true
                color: Kirigami.Theme.textColor
            }

            GridLayout {
                columns: win.cases.length + 1
                columnSpacing: 14
                rowSpacing: 8

                Text { text: ""; color: Kirigami.Theme.textColor }
                Repeater {
                    model: win.cases
                    Text {
                        required property var modelData
                        text: modelData.title
                        color: Kirigami.Theme.textColor
                        font.pixelSize: 11
                        Layout.preferredWidth: 96
                        wrapMode: Text.Wrap
                    }
                }

                // Rows: 22 px zoomed 4x (pixel-exact), then 32 and 48 px actual size.
                Repeater {
                    model: [{ size: 22, zoom: 4, label: "22 px\n(4x zoom)" }, { size: 32, zoom: 1, label: "32 px" }, { size: 48, zoom: 1, label: "48 px" }]
                    delegate: Repeater {
                        id: sizeRow
                        required property var modelData
                        model: win.cases.length + 1
                        delegate: Item {
                            required property int index
                            readonly property var theCase: index > 0 ? win.cases[index - 1] : null
                            Layout.preferredWidth: Math.max(96, sizeRow.modelData.size * sizeRow.modelData.zoom)
                            Layout.preferredHeight: sizeRow.modelData.size * sizeRow.modelData.zoom

                            Text {
                                visible: index === 0
                                text: sizeRow.modelData.label
                                color: Kirigami.Theme.textColor
                                font.pixelSize: 11
                            }

                            Item {
                                visible: index > 0
                                width: sizeRow.modelData.size
                                height: sizeRow.modelData.size
                                transformOrigin: Item.TopLeft
                                scale: sizeRow.modelData.zoom
                                layer.enabled: sizeRow.modelData.zoom > 1
                                layer.smooth: false
                                layer.textureSize: Qt.size(width, height)

                                Loader {
                                    anchors.fill: parent
                                    active: theCase !== null
                                    sourceComponent: UI.CompactRepresentation {
                                        xkb: win.makeState(theCase.s)
                                        plasmoidItem: fakePlasmoid.createObject(win, { xkb: xkb })
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Text {
                text: "Popup (click on the tray icon):"
                color: Kirigami.Theme.textColor
            }
            Rectangle {
                color: Kirigami.Theme.backgroundColor
                border.color: Qt.rgba(0.5, 0.5, 0.5, 0.5)
                implicitWidth: popup.Layout.preferredWidth
                implicitHeight: popup.Layout.preferredHeight
                UI.FullRepresentation {
                    id: popup
                    anchors.fill: parent
                    xkb: win.makeState({ shift: win.latched, control: win.locked, meta: win.held })
                    plasmoidItem: fakePlasmoid.createObject(win, { xkb: xkb })
                }
            }
        }
    }

    RowLayout {
        id: gallery
        spacing: 0

        ThemePanel {
            themeName: "Light theme"
            Kirigami.Theme.inherit: false
            Kirigami.Theme.textColor: "#232629"
            Kirigami.Theme.backgroundColor: "#eff0f1"
            Kirigami.Theme.highlightColor: "#3daee9"
            Kirigami.Theme.highlightedTextColor: "#ffffff"
        }
        ThemePanel {
            themeName: "Dark theme"
            Kirigami.Theme.inherit: false
            Kirigami.Theme.textColor: "#fcfcfc"
            Kirigami.Theme.backgroundColor: "#202326"
            Kirigami.Theme.highlightColor: "#3daee9"
            Kirigami.Theme.highlightedTextColor: "#fcfcfc"
        }
    }

    // Give layouts and layers a moment to settle, then save (grabToImage renders).
    Timer {
        interval: 500
        running: true
        onTriggered: gallery.grabToImage(result => {
            if (!result.saveToFile(outputPath)) {
                console.error("could not write", outputPath);
                Qt.exit(1);
                return;
            }
            console.info("wrote", outputPath);
            Qt.quit();
        })
    }
}
