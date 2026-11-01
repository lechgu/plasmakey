// Pure (X-independent) logic: which real modifier bits mean Shift/Ctrl/Alt/Meta,
// and how XKB's depressed/latched/locked masks map onto one modifier.
#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace PlasmaKey {

// The eight X11 real modifiers in protocol order: Shift, Lock, Control, Mod1..Mod5.
inline constexpr int RealModifierCount = 8;

// For each real modifier, the keysyms of the keys bound to it
// (the same table `xmodmap -pm` prints).
using ModifierKeysyms = std::array<std::vector<uint32_t>, RealModifierCount>;

// Real-modifier bits for the modifiers the indicator shows. 0 = not mapped.
struct ModifierMasks {
    uint8_t shift = 0;
    uint8_t control = 0;
    uint8_t alt = 0;
    uint8_t meta = 0;

    bool operator==(const ModifierMasks &) const = default;
};

// Resolves masks from the active keymap, using the same rules as Qt's X11
// backend, so "Meta" is what KDE shortcuts call Meta (normally the Super key).
ModifierMasks resolveModifierMasks(const ModifierKeysyms &rows);

enum ModifierFlag : int {
    Inactive = 0,
    Depressed = 1 << 0, // physically held down
    Latched = 1 << 1, // one-shot: applies to the next key, then clears
    Locked = 1 << 2, // stays on until pressed again
};

// Flags for one modifier, given XKB's base (depressed), latched and locked masks.
int modifierFlags(uint8_t mask, uint8_t base, uint8_t latched, uint8_t locked);

// "Shift", "Lock", "Control", "Mod1".."Mod5".
const char *realModifierName(int index);

// Name of a modifier-related keysym, or nullptr if not in our small table.
const char *keysymName(uint32_t keysym);

// Keysym values are fixed by the X11 protocol (X11/keysymdef.h).
namespace Keysym {
inline constexpr uint32_t Shift_L = 0xffe1;
inline constexpr uint32_t Shift_R = 0xffe2;
inline constexpr uint32_t Control_L = 0xffe3;
inline constexpr uint32_t Control_R = 0xffe4;
inline constexpr uint32_t Caps_Lock = 0xffe5;
inline constexpr uint32_t Shift_Lock = 0xffe6;
inline constexpr uint32_t Meta_L = 0xffe7;
inline constexpr uint32_t Meta_R = 0xffe8;
inline constexpr uint32_t Alt_L = 0xffe9;
inline constexpr uint32_t Alt_R = 0xffea;
inline constexpr uint32_t Super_L = 0xffeb;
inline constexpr uint32_t Super_R = 0xffec;
inline constexpr uint32_t Hyper_L = 0xffed;
inline constexpr uint32_t Hyper_R = 0xffee;
inline constexpr uint32_t ISO_Level3_Shift = 0xfe03;
inline constexpr uint32_t ISO_Level5_Shift = 0xfe11;
inline constexpr uint32_t Mode_switch = 0xff7e;
inline constexpr uint32_t Num_Lock = 0xff7f;
inline constexpr uint32_t Scroll_Lock = 0xff14;
} // namespace Keysym

} // namespace PlasmaKey
