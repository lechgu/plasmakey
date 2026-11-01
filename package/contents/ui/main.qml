// plasmakey: a system tray item showing the X server's XKB state
// for Shift, Ctrl, Alt and Meta (off / held / latched / locked).
// Read-only: it never changes keyboard state or settings.
import QtQuick
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore
import plasmakey.xkb as Xkb

PlasmoidItem {
    id: root

    readonly property Xkb.ModifierState xkb: Xkb.ModifierState {}

    // In keyboard order, as drawn in the icon.
    readonly property var modifiers: [
        { name: i18nc("@label modifier key", "Shift"), flags: xkb.shift, mapped: true },
        { name: i18nc("@label modifier key", "Ctrl"), flags: xkb.control, mapped: true },
        { name: i18nc("@label modifier key", "Meta"), flags: xkb.meta, mapped: xkb.metaMapped },
        { name: i18nc("@label modifier key", "Alt"), flags: xkb.alt, mapped: xkb.altMapped }
    ]

    function stateName(flags) {
        if (flags & Xkb.ModifierState.Locked)
            return i18nc("modifier state", "locked");
        if (flags & Xkb.ModifierState.Latched)
            return i18nc("modifier state", "latched");
        if (flags & Xkb.ModifierState.Depressed)
            return i18nc("modifier state", "held");
        return i18nc("modifier state", "off");
    }

    Plasmoid.icon: "preferences-desktop-accessibility"
    // Always visible in the tray, like the Windows indicator.
    Plasmoid.status: PlasmaCore.Types.ActiveStatus

    toolTipMainText: Plasmoid.title // "plasmakey", from metadata.json
    toolTipSubText: {
        if (!xkb.supported || !xkb.connected)
            return xkb.problem;
        const lines = [];
        for (const m of modifiers) {
            if (m.flags !== 0)
                lines.push(i18nc("modifier: state", "%1: %2", m.name, stateName(m.flags)));
        }
        if (lines.length === 0)
            lines.push(i18n("No modifier keys latched or locked"));
        if (xkb.stickyKeysKnown && !xkb.stickyKeysEnabled)
            lines.push(i18n("Sticky Keys is turned off"));
        lines.push("");
        lines.push(i18n("Bold outline = latched (next key only)"));
        lines.push(i18n("Filled = locked (press the key again to release)"));
        return lines.join("\n");
    }

    compactRepresentation: CompactRepresentation {
        plasmoidItem: root
        xkb: root.xkb
    }

    fullRepresentation: FullRepresentation {
        plasmoidItem: root
        xkb: root.xkb
    }
}
