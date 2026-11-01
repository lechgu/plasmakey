# Tests: checklist and results

Environment: Debian 13.7, Xorg 21.1.16 (X11 session), plasmashell 6.3.6,
libplasma 6.3.5, KF6 6.13, Qt 6.8.2, libxcb-xkb 1.17.0. Keymap us,pl; Sticky
Keys on, "Lock sticky keys" on. Results below are from 2026-11-01.

Status legend: **verified (auto)** = by a test in this repo; **verified
(screen)** = checked on screenshots of the real panel; **not verified** =
needs the manual steps given, by a person at the keyboard.

## Automated

| Command | What it checks | Result |
|---|---|---|
| `make test` | mask resolution for several keymaps (default, Alt/Meta on unusual modifiers, Meta vs Super/Hyper rules, unmapped keys) and per-modifier flags | 10/10 test cases pass |
| `make test-live` | end-to-end through the same `XkbWatcher` the widget uses, on the live X server (see below) | 30/30 pass, every update < 1 ms |
| `make preview` | renders the real QML: icon in 8 states at 22/32/48 px, light and dark theme, plus the popup → `build/preview.png` (copy in `docs/preview.png`) | checked by eye |

`make test-live` steps (all passed):

1. Lock, unlock, latch and clear-latch for **Shift, Ctrl, Alt, Meta**, each
   via XKB requests from another X client. Covers "changes made outside the
   widget are reflected without restarting it".
2. **Simultaneous**: Ctrl+Shift latched, then Alt locked on top, then Alt
   unlocked. Each modifier is reported independently.
3. **Real Sticky Keys** via simulated keys (xdotool/XTest), for Shift and Ctrl:
   key held → *held* only (not latched or locked); released → *latched*;
   tapped again → *locked*; still locked 300 ms later; tapped a third time → off.
4. The original state is restored at the end and checked.

Alt and Meta are not tapped in step 3, because tapping Meta opens the
application launcher and Alt can open menu bars in the focused window.

## Acceptance checklist

| # | Check | Status |
|---|---|---|
| 1 | Press and release Ctrl → **latched**; clears after a non-modifier key | latch: verified (auto, step 3). Clearing by a real key: **not verified**, see M1 |
| 2 | Press Ctrl twice → **locked**; stays after release and across other keys; clears when pressed again | verified (auto, step 3) for Shift and Ctrl; "across other keys": **not verified**, see M2 |
| 3 | Same for Shift, Alt, Meta | Shift: verified (auto). Alt, Meta: state display verified (auto, step 1); physical keys **not verified**, see M3 |
| 4 | Holding a modifier is not shown as latched or locked | verified (auto, step 3) |
| 5 | Simultaneous modifiers shown independently | verified (auto, step 2) |
| 6 | Changes made outside the widget are reflected without restarting it | XKB-state changes: verified (auto, step 1). Turning Sticky Keys on/off in System Settings: **not verified**, see M4 |
| 7 | Lives in the system tray, visible without opening anything | verified (screen): added to both panels' trays automatically, in the visible area next to the `^` arrow; idle, locked and latched looks confirmed in the real tray |
| 8 | Tooltip | **not verified**, see M5 |
| 9 | Popup | layout verified (preview); in the real tray **not verified**, see M5 |
| 10 | Theme light/dark | light: verified (screen, real panel). dark: verified (preview only), see M6 |
| 11 | Scaling / HiDPI | 22/32/48 px verified (preview). Real fractional scaling **not verified**, see M7 |
| 12 | Survives a panel restart | verified: `make reload` restarted plasmashell three times; the indicator came back each time (no errors in its log after the first-run fix) |
| 13 | Survives logout/login | **not verified**, see M8 |
| 14 | Clean uninstall | verified (auto): install + uninstall into a scratch prefix leaves no files. On the real system **not verified**, see M9 |
| 15 | Wayland shows "not supported" instead of state | rendering verified (preview); in a real Wayland session **not verified**, see M10 |
| 16 | Reproducible build | verified: fresh copy of the sources → `make` + `make test` pass; `make deps` prints the apt command for missing packages; in-source CMake builds are refused (they would overwrite the Makefile) |

## Manual steps (for the "not verified" items)

Keep `make diag` running in a terminal as the reference: it prints every XKB
state change. The tray icon must always agree with it.

- **M1, latch cleared by a key**: open an empty text editor. Tap Shift once:
  Shift shows a bold outline. Type `a`: `A` appears and the outline
  disappears. Repeat with Ctrl, then `a` (select all, harmless in an empty
  document): Ctrl clears.
- **M2, lock persists**: in the editor, tap Shift twice: Shift is solid. Type
  `abc` (`ABC` appears): still solid. Tap Shift once: off. For Ctrl: tap twice
  (solid), press Left/Right arrows a few times (moves by word): still solid;
  tap Ctrl: off.
- **M3, Alt and Meta**: tap Alt once (bold outline), again (solid), again
  (off). Same for Meta. Note that KDE may open the launcher on a Meta tap.
- **M4, settings changes**: System Settings → Accessibility → Modifier Keys.
  Untick *Use sticky keys* and apply: the icon dims and the tooltip says Sticky
  Keys is off. Tick it again: normal. Untick *Lock sticky keys*: the popup's
  last line says locking is off.
- **M5, tooltip and popup**: hover over the icon with Ctrl latched: the tooltip
  lists "Ctrl: latched" plus the legend. Click: the popup shows the labelled
  keys, per-key state and the legend; click again to close.
- **M6, dark theme**: System Settings → Colors & Themes → Global Theme →
  Breeze Dark. The icon redraws in light-on-dark colors.
- **M7, scaling**: System Settings → Display → Scale 150% (log out/in on X11).
  The keys stay crisp and the states stay distinguishable.
- **M8, login**: log out and back in. The icon is in the tray and reacts to
  Shift taps. `systemctl --user show-environment | grep QML_IMPORT_PATH`
  shows `~/.local/lib/qml`.
- **M9, uninstall**: `make uninstall && make reload`. The icon is gone, and
  `ls ~/.local/share/plasma/plasmoids ~/.local/lib/qml ~/.config/plasma-workspace/env`
  shows nothing from plasmakey. Reinstall with `make install && make reload`.
- **M10, Wayland**: log into a "Plasma (Wayland)" session. The icon is
  dimmed with a warning sign; hovering says it is not supported (X11 only).
