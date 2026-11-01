# AGENTS.md — Plasma Sticky Keys Status Indicator

## Mission
Build a small, native-looking **KDE Plasma 6 system tray indicator** for **Debian 13, KDE Plasma 6, X11**. It must display the **actual XKB modifier state** of Sticky Keys, especially modifiers accidentally **latched** or **locked**. The user already uses KDE Sticky Keys and *Lock sticky keys* and wants to keep both enabled. The UX should be as legible and unobtrusive as the familiar Windows/macOS accessibility indicators, but integrated into the **Plasma system tray** rather than a separate window.

Do not reinterpret this as a keyboard layout switcher, Caps Lock indicator, keypress visualizer, or generic keyboard monitor.

## Non-negotiable product requirements
1. The indicator lives **inside Plasma's system tray** on the desktop panel, using supported Plasma 6 mechanisms. No floating window, dock, separate taskbar application, or persistent terminal.
2. Show **Shift, Ctrl, Alt, Meta/Super** separately. Prefer short labels (`⇧`, `Ctrl`, `Alt`, `Meta`) with accessible text/tooltip. Use the active keyboard map to identify modifier masks; **never assume fixed XKB bit positions**.
3. Represent at least four states: **inactive**, **physically depressed**, **latched** (one-shot), and **locked** (persistent). Latched and locked MUST be visually distinguishable; color alone is insufficient. For example, solid highlight = locked, outlined highlight = latched, subtle pressed styling = depressed. Document final semantics in a tooltip.
4. State must reflect the X server's **effective XKB state**, not an inferred count of keydown/keyup events. In particular, a locked modifier must remain visibly locked after the physical key is released; a latched modifier must clear when XKB consumes it.
5. Changes should appear promptly, without polling if XKB state notifications can be subscribed to reliably. Handle reconnects and X server errors gracefully.
6. Do not toggle, unlock, enable, disable, or otherwise modify Sticky Keys, modifier maps, keyboard layouts, accessibility settings, or the user's keypresses. **Read-only observer.**
7. No permanent tray clutter when all modifiers are inactive: use a small neutral icon or a configurable hide-when-idle behavior **only if compatible with Plasma tray visibility rules**. Active modifiers must be immediately visible without opening a popup. Ensure the system tray does not silently hide active status under its overflow menu; document how to set 'Always Shown' if Plasma requires it.
8. Compact, high-DPI-aware, theme-aware (light/dark, panel orientation), using Plasma theme colors and icons. No hardcoded bright green matrix, no large grid, no notifications on every keystroke.
9. The widget must survive Plasma panel restarts and user logins and must not require root privileges.
10. Target **X11 only** for MVP. Detect unsupported Wayland sessions and show a clear unsupported message rather than displaying misleading state.

## Architecture and technical constraints
- **First investigate the Plasma 6 API and packaging on Debian 13** before choosing an implementation. Prefer a native Plasma 6 plasmoid (Qt/QML, `org.kde.plasma.plasmoid`, supported metadata/package structure) for the panel/tray UI.
- XKB state can be read using **Xlib XKB** (`XkbGetState`, `XkbStateNotify`), or a correct XCB/XKB equivalent. Inspect depressed, latched, and locked modifier masks independently; `mods`/effective state alone is insufficient.
- If QML cannot access the XKB API directly, implement a **small unprivileged local backend** (prefer C++/Qt or a tiny C++ executable), and connect it to the plasmoid using a supported, maintainable IPC mechanism such as session D-Bus. Avoid launching `xkbwatch`, parsing `xset` text, or reading `/dev/input`.
- Check whether Plasma's system tray supports hosting the plasmoid directly; if a standalone StatusNotifierItem host/helper is a better-supported route, document the tradeoff and obtain the same visual behavior. **Do not claim a normal panel widget is automatically a system tray item.**
- Do not add Python, Electron, browser runtimes, network services, privileged daemons, or dependencies unless there is a compelling technical reason.
- Never invent Plasma API names or packaging commands. Verify against the actual installed Plasma 6 environment and available development headers/documentation.
- Consider layout changes, multiple keyboards, and modifier aliases. Distinguish actual XKB modifier masks from display names. Handle cases where Alt/Meta mappings differ from defaults.

## Desired appearance
- At rest: a quiet, compact accessibility/keyboard glyph in the tray, optionally hidden by configuration.
- Active: readable short modifier labels in a horizontal strip if space permits, or a single compact tray icon with a clear badge plus immediate hover tooltip. **The active state must be legible at a glance**; do not rely exclusively on a tooltip or popup.
- Example conceptual states (not prescribed exact artwork): `Ctrl` outlined = latched; `Ctrl` filled = locked; `⇧` subtle pressed = held; inactive modifiers dim or absent.
- A popup may provide detailed state and a small legend, but should not be required to identify a locked modifier.
- Match KDE's native panel spacing, hover behavior, and typography; avoid custom decoration.

## Development workflow
1. Inspect the local environment: `plasmashell --version`, `echo "$XDG_SESSION_TYPE"`, relevant Plasma packages, XKB libraries, and development tools. Report findings in README.
2. Produce a short architecture note describing how XKB events become UI state and how the item integrates into the **system tray**.
3. Implement the smallest end-to-end vertical slice: one modifier state read from XKB, displayed in the tray, updated on state changes.
4. Extend to all four modifiers and all four states; add theme and accessibility polish only after correctness.
5. Provide a reproducible build/install/uninstall workflow, preferably via CMake and user-local installation where supported. Do not overwrite KDE system files.
6. Document how to add/enable the indicator and force it to **Always Shown** in Plasma's system tray configuration if needed.
7. Include a small diagnostic mode that prints XKB masks and mapped modifier names to aid troubleshooting, but keep it disabled in normal use.

## Acceptance tests (must demonstrate, not merely assert)
- Press and release Ctrl once with Sticky Keys enabled: Ctrl is **latched**, then clears after a non-modifier key consumes it.
- Press Ctrl twice as configured by KDE: Ctrl becomes **locked**, remains shown after release and across unrelated keypresses, then clears when unlocked.
- Repeat for Shift, Alt, and Meta where the current KDE/XKB configuration supports them.
- Holding a modifier physically must not be misrepresented as latched or locked.
- Simultaneous modifiers, e.g. Ctrl+Shift, show each independently.
- State changes initiated outside the widget (including KDE accessibility settings) are reflected without restarting it.
- Tray visibility, tooltip, theme, scaling, panel restart, login restart, and clean uninstall are verified.
- Test XKB state using `xkbwatch` or a diagnostic XKB client as a reference, not as the production UI.
- If any behavior cannot be tested automatically, provide exact manual reproduction steps and mark it **not verified** until tested.

## Deliverables
- Working source code with conventional project layout.
- `README.md`: purpose, dependencies, build, install, enable in tray, configure Always Shown, uninstall, troubleshooting.
- `docs/ARCHITECTURE.md`: XKB masks/events, modifier mapping, UI/backend integration, limitations.
- A concise test checklist and results with environment versions.
- No unrelated features, no telemetry, no network access, no privilege escalation.

## Definition of done
Done means an actual, usable **Plasma 6 system tray indicator** on Debian 13/X11 that accurately and immediately shows **latched vs locked** modifier states. A standalone demo window, generic keyboard LED indicator, keypress overlay, or unlabeled XKB matrix does **not** satisfy this specification.
