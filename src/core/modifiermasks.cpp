#include "modifiermasks.h"

namespace PlasmaKey {

ModifierMasks resolveModifierMasks(const ModifierKeysyms &rows)
{
    uint8_t shift = 0, control = 0, alt = 0, meta = 0, super = 0, hyper = 0;

    for (int i = 0; i < RealModifierCount; ++i) {
        const auto bit = static_cast<uint8_t>(1u << i);
        for (const uint32_t sym : rows[i]) {
            switch (sym) {
            case Keysym::Shift_L:
            case Keysym::Shift_R:
                shift |= bit;
                break;
            case Keysym::Control_L:
            case Keysym::Control_R:
                control |= bit;
                break;
            case Keysym::Alt_L:
            case Keysym::Alt_R:
                alt |= bit;
                break;
            case Keysym::Meta_L:
            case Keysym::Meta_R:
                meta |= bit;
                break;
            case Keysym::Super_L:
            case Keysym::Super_R:
                super |= bit;
                break;
            case Keysym::Hyper_L:
            case Keysym::Hyper_R:
                hyper |= bit;
                break;
            default:
                break;
            }
        }
    }

    // Same conflict rules as Qt's X11 keyboard code (QXcbKeyboard): a Meta
    // keysym that shares Alt's modifier is not a separate Meta; then fall back
    // to Super, then Hyper. Most keymaps put Meta_L on Mod1 with Alt, so
    // "Meta" ends up as the Super/Windows key, matching KDE.
    if (meta == alt)
        meta = 0;
    if (meta == 0)
        meta = super;
    if (meta == 0)
        meta = hyper;

    ModifierMasks masks;
    // Shift and Control are named by the X protocol itself; keep those bits
    // if no key is bound to them so a lock set by other means still shows.
    masks.shift = shift ? shift : 0x01;
    masks.control = control ? control : 0x04;
    masks.alt = alt;
    masks.meta = meta;
    return masks;
}

int modifierFlags(uint8_t mask, uint8_t base, uint8_t latched, uint8_t locked)
{
    if (mask == 0)
        return Inactive;
    int flags = Inactive;
    if (base & mask)
        flags |= Depressed;
    if (latched & mask)
        flags |= Latched;
    if (locked & mask)
        flags |= Locked;
    return flags;
}

const char *realModifierName(int index)
{
    static const char *const names[RealModifierCount] = {"Shift", "Lock", "Control", "Mod1", "Mod2", "Mod3", "Mod4", "Mod5"};
    return (index >= 0 && index < RealModifierCount) ? names[index] : "?";
}

const char *keysymName(uint32_t keysym)
{
    switch (keysym) {
    case Keysym::Shift_L: return "Shift_L";
    case Keysym::Shift_R: return "Shift_R";
    case Keysym::Control_L: return "Control_L";
    case Keysym::Control_R: return "Control_R";
    case Keysym::Caps_Lock: return "Caps_Lock";
    case Keysym::Shift_Lock: return "Shift_Lock";
    case Keysym::Meta_L: return "Meta_L";
    case Keysym::Meta_R: return "Meta_R";
    case Keysym::Alt_L: return "Alt_L";
    case Keysym::Alt_R: return "Alt_R";
    case Keysym::Super_L: return "Super_L";
    case Keysym::Super_R: return "Super_R";
    case Keysym::Hyper_L: return "Hyper_L";
    case Keysym::Hyper_R: return "Hyper_R";
    case Keysym::ISO_Level3_Shift: return "ISO_Level3_Shift";
    case Keysym::ISO_Level5_Shift: return "ISO_Level5_Shift";
    case Keysym::Mode_switch: return "Mode_switch";
    case Keysym::Num_Lock: return "Num_Lock";
    case Keysym::Scroll_Lock: return "Scroll_Lock";
    default: return nullptr;
    }
}

} // namespace PlasmaKey
