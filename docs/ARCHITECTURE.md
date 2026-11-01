# Architecture

## Overview

```
 X server (XKB)
   │  StateNotify / MapNotify / NewKeyboardNotify / ControlsNotify   (events, no polling)
   ▼
 XkbWatcher  (src/core, C++/XCB, own X connection, read-only)
   │  base / latched / locked masks, keymap → masks, Sticky Keys controls
   ▼
 ModifierState  (src/qml, QML module "plasmakey.xkb", loaded in plasmashell)
   │  per-modifier flags: Depressed | Latched | Locked
   ▼
 plasmoid "plasmakey"  (package/, QML)
   ├─ compact representation = the tray icon (four-key glyph)
   ├─ tooltip (plain text)
   └─ full representation = popup (state in words + legend)
```

Everything runs inside `plasmashell`, the process that draws the panel and the
system tray. There is no daemon, no D-Bus service, no polling, no root.

## Why this shape

**Tray integration.** Plasma 6's system tray hosts *plasmoids* that declare
`X-Plasma-NotificationAreaCategory` in their `metadata.json`, which is how
KDE's own tray items (battery, volume, "Lock Keys Status") work. A tray-hosted
plasmoid gets the tray's native spacing, hover and popup behavior, and Plasma
theme colors. `"EnabledByDefault": true` makes the tray add it automatically
the first time it sees it. The tray gives every item one square icon cell,
which is why the state is drawn as a compact glyph rather than a strip of
labels.

The alternative, a standalone StatusNotifierItem program, also shows in the
tray, but it can only show a pixmap and a menu. It would have needed its own
autostart, and the result is less native. It was not needed.

**Reading XKB.** QML cannot talk to X11, so a small C++ part is required. It
is a QML module loaded into plasmashell, the same pattern as KDE's own
`org.kde.plasma.private.keyboardindicator`. Alternatives considered:

- *Separate helper process + session D-Bus*: Plasma 6.3's QML D-Bus module
  (`org.kde.plasma.workspace.dbus`) can call methods but cannot subscribe to
  signals, so the widget would have to poll or long-poll. It would also add a
  second process to start and supervise.
- *C++ applet plugin (`Plasma::Applet`)*: needs `libplasma-dev` and gains
  nothing here; Plasma 6.3's CMake package has no helper for building one
  (only `plasma_install_package`).

Trade-off of the chosen design: a user-local QML module is only found if
`QML_IMPORT_PATH` includes it. `make install` adds that through Plasma's
standard per-user env directory (`~/.config/plasma-workspace/env/`), and
`make reload` applies it to the running session. Because the code runs inside
plasmashell, it is kept small, never blocks, and does its X work on a
separate connection with XCB, which has no process-global error handlers
(unlike Xlib).

## XKB: events and state

`XkbWatcher` (`src/core/xkbwatcher.cpp`):

1. Opens its **own** XCB connection (`xcb_connect`), separate from Qt's, so
   its event selection cannot interfere with Qt's keyboard handling.
2. Enables XKB (`xcb_xkb_use_extension`) and **subscribes before reading**,
   so no change can slip in between the first read and the subscription:
   - `StateNotify`, only for the *modifier base / latch / lock* components
     (not group or pointer-button changes);
   - `MapNotify`, `NewKeyboardNotify` (keymap or keyboard changes) and the
     core `MappingNotify` (e.g. `xmodmap`): re-resolve the masks;
   - `ControlsNotify`: re-read the Sticky Keys settings.
3. Reads the initial state with `xcb_xkb_get_state` and controls with
   `xcb_xkb_get_controls`, for the core keyboard (`XCB_XKB_ID_USE_CORE_KBD`).
4. Watches the connection's socket with a `QSocketNotifier` and drains all
   queued events each time (also after its own replies, which can pull events
   into XCB's queue without making the socket readable again).

The three XKB modifier components are used **independently**:

| XKB field | Meaning | Shown as |
|---|---|---|
| `baseMods` | keys physically held down | held |
| `latchedMods` | Sticky Keys latch: applies to the next key, then XKB clears it | latched |
| `lockedMods` | Sticky Keys lock (press twice), Caps/Num Lock, … | locked |

The effective `mods` (what applications see) is kept only for diagnostics; it
can't tell latched from locked. When several components apply, the icon shows
the strongest: locked > latched > held.

Sticky Keys settings read from `GetControls`: `enabledControls & STICKY_KEYS`
("Use sticky keys"), `accessXOption & LATCH_TO_LOCK` ("Lock sticky keys"),
`accessXOption & TWO_KEYS` ("turn off when two keys pressed").

**Errors and reconnects.** If the connection fails or breaks
(`xcb_connection_has_error`), state is cleared, the widget shows a warning,
and the watcher retries with back-off (1 s doubling to 30 s). An X server
without XKB is reported once and not retried. Under Wayland the QML module
refuses to start (`QGuiApplication::platformName() != "xcb"`): Xwayland's
XKB state is not the real keyboard's, so the widget shows "not supported"
instead of misleading state.

## Modifier mapping (no fixed bit positions)

Which real modifier (Shift, Lock, Control, Mod1–Mod5) means "Alt" or "Meta"
depends on the keymap. `resolveModifierMasks` (`src/core/modifiermasks.cpp`)
reads the **active** modifier map (`GetModifierMapping`) plus each modifier's
keysyms (`GetKeyboardMapping`), the same table `xmodmap -pm` prints, which
the server derives from the XKB keymap. Then:

- Shift = modifiers holding `Shift_L`/`Shift_R`; Ctrl = `Control_L`/`Control_R`;
  Alt = `Alt_L`/`Alt_R`.
- Meta follows **Qt's rules** (`QXcbKeyboard::resolveMaskConflicts`), so it is
  exactly what KDE shortcuts call Meta: a `Meta_L/R` keysym counts only if its
  modifier differs from Alt's; otherwise use Super's modifier, else Hyper's.
  In the stock keymap `Meta_L` shares Mod1 with Alt, so Meta = the Super
  (Windows) key's Mod4.
- If no key is bound to Shift or Control, the protocol's own Shift/Control
  bits are used so a lock set some other way still shows. A missing Alt or
  Meta is drawn faded ("not mapped").

Example from the development machine (`make diag`):

```
  Shift    0x01  Shift_L Shift_R
  Control  0x04  Control_L Control_R
  Mod1     0x08  Alt_L Meta_L Alt_R Meta_R ISO_Level3_Shift
  Mod4     0x40  Super_L Super_R Hyper_L
  Mod5     0x80  ISO_Level3_Shift
→ Shift = 0x01, Ctrl = 0x04, Alt = 0x08 (Mod1), Meta = 0x40 (Mod4)
```

Layout switching (e.g. us ↔ pl) changes the XKB *group*, not the modifier
map, so the masks stay valid. Keymap reloads (`setxkbmap`, KDE keyboard
settings, plugging in a keyboard with a different map) trigger
`MapNotify`/`NewKeyboardNotify` and the masks are recomputed. AltGr
(`ISO_Level3_Shift`, Mod5 here) is deliberately not shown.

## UI

- **Tray icon** (`CompactRepresentation.qml`, `KeyboardGlyph.qml`,
  `KeyGlyph.qml`): four keys drawn with QML rectangles in whole pixels, so
  they stay crisp at any panel size and scale factor. Colors come from
  `Kirigami.Theme` (panel text color, theme highlight color), so the icon
  follows light and dark themes. States differ by **shape**: thin outline (off),
  translucent fill (held), bold hollow outline at least 2 px (latched), solid
  fill (locked). Labels appear only when they fit at a readable size (the
  popup, very large panels).
- **Always visible**: `Plasmoid.status` is always `ActiveStatus`, so with the
  tray's default *Shown when relevant* it stays in the visible tray area.
- **Tooltip**: active modifiers in words, plus a one-line legend.
- **Popup**: larger labelled glyph, per-modifier state, legend, Sticky Keys
  settings.
- **Problems** (Wayland, no X connection): dimmed glyph plus a warning
  emblem; the tooltip and popup give the reason.

## Limitations

- X11 only (by design for this version).
- State is the **core keyboard's** XKB state. With several physical keyboards,
  Xorg merges them into that one state, which is also what applications use.
- The tray cell is small: at ~22 px the keys are ~6 px wide and unlabelled;
  identification relies on position (keyboard order) plus tooltip/popup.
- Keysym lookup uses the core keyboard mapping, which covers XKB groups 1–2.
  A modifier keysym that only exists in groups 3–4 of an unusual keymap would
  not be seen.
- The QML module runs inside plasmashell, so a crash in it would take the
  panel down. The code is small, doesn't block, and is covered by tests.
- `QML_IMPORT_PATH` is set for the whole Plasma session (it only adds
  `~/.local/lib/qml`).
- Not translated yet (strings are wrapped in `i18n()`; no catalogs shipped).
- Xorg quirk seen while testing: clearing *one* latched modifier through an
  XKB request (`XkbLatchModifiers`) clears *all* latches. That is server
  behavior; the indicator shows it correctly.
