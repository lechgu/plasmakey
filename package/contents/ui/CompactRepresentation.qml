// The tray icon: the four-key glyph, readable without opening anything.
import QtQuick
import org.kde.kirigami as Kirigami

MouseArea {
    id: compact

    required property var plasmoidItem
    required property var xkb

    readonly property bool working: xkb.supported && xkb.connected

    hoverEnabled: true
    activeFocusOnTab: true

    Accessible.role: Accessible.Button
    Accessible.name: plasmoidItem.toolTipMainText
    Accessible.description: plasmoidItem.toolTipSubText

    property bool wasExpanded: false
    onPressed: wasExpanded = plasmoidItem.expanded
    onClicked: plasmoidItem.expanded = !wasExpanded
    Keys.onPressed: event => {
        if (event.key === Qt.Key_Space || event.key === Qt.Key_Enter || event.key === Qt.Key_Return || event.key === Qt.Key_Select) {
            plasmoidItem.expanded = !plasmoidItem.expanded;
            event.accepted = true;
        }
    }

    KeyboardGlyph {
        anchors.centerIn: parent
        size: Math.min(compact.width, compact.height)
        shiftFlags: compact.xkb.shift
        controlFlags: compact.xkb.control
        metaFlags: compact.xkb.meta
        altFlags: compact.xkb.alt
        metaMapped: compact.xkb.metaMapped
        altMapped: compact.xkb.altMapped
        // Dim when the state cannot be trusted or Sticky Keys is off.
        opacity: !compact.working ? 0.3 : (compact.xkb.stickyKeysKnown && !compact.xkb.stickyKeysEnabled) ? 0.6 : 1
    }

    Kirigami.Icon {
        visible: !compact.working
        source: "dialog-warning"
        width: Math.round(Math.min(compact.width, compact.height) * 0.55)
        height: width
        anchors.right: parent.right
        anchors.bottom: parent.bottom
    }
}
