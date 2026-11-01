// Unit tests for the X-independent logic: modifier-mask resolution from a
// keymap, and per-modifier flags from XKB state masks. No X server needed.

#include "modifiermasks.h"

#include <QTest>

using namespace PlasmaKey;

namespace {

// Debian/KDE default with us,pl layouts, as reported by `xmodmap -pm`.
ModifierKeysyms defaultKeymap()
{
    ModifierKeysyms rows;
    rows[0] = {Keysym::Shift_L, Keysym::Shift_R};
    rows[1] = {Keysym::Caps_Lock};
    rows[2] = {Keysym::Control_L, Keysym::Control_R};
    rows[3] = {Keysym::Alt_L, Keysym::Alt_R, Keysym::Meta_L}; // Mod1
    rows[4] = {Keysym::Num_Lock}; // Mod2
    rows[5] = {Keysym::ISO_Level5_Shift}; // Mod3
    rows[6] = {Keysym::Super_L, Keysym::Super_R, Keysym::Hyper_L}; // Mod4
    rows[7] = {Keysym::ISO_Level3_Shift}; // Mod5
    return rows;
}

} // namespace

class TestModifierMasks : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void defaultKeymap_resolvesKdeMetaToSuper()
    {
        const ModifierMasks m = resolveModifierMasks(defaultKeymap());
        QCOMPARE(m.shift, uint8_t(0x01));
        QCOMPARE(m.control, uint8_t(0x04));
        QCOMPARE(m.alt, uint8_t(0x08)); // Mod1
        QCOMPARE(m.meta, uint8_t(0x40)); // Mod4 (Super), not Mod1 where Meta_L sits with Alt
    }

    void altOnUnusualModifier_followsKeymap()
    {
        // No fixed bit positions: Alt on Mod3, Super on Mod2.
        ModifierKeysyms rows;
        rows[0] = {Keysym::Shift_L};
        rows[2] = {Keysym::Control_L};
        rows[5] = {Keysym::Alt_L, Keysym::Alt_R};
        rows[4] = {Keysym::Super_L};
        const ModifierMasks m = resolveModifierMasks(rows);
        QCOMPARE(m.alt, uint8_t(0x20));
        QCOMPARE(m.meta, uint8_t(0x10));
    }

    void separateMetaKeysym_isPreferredOverSuper()
    {
        // Like Qt: a Meta keysym on its own modifier is Meta; Super stays separate.
        ModifierKeysyms rows = defaultKeymap();
        rows[3] = {Keysym::Alt_L, Keysym::Alt_R};
        rows[5] = {Keysym::Meta_L, Keysym::Meta_R};
        const ModifierMasks m = resolveModifierMasks(rows);
        QCOMPARE(m.alt, uint8_t(0x08));
        QCOMPARE(m.meta, uint8_t(0x20));
    }

    void noSuper_fallsBackToHyper()
    {
        ModifierKeysyms rows = defaultKeymap();
        rows[6] = {Keysym::Hyper_L};
        QCOMPARE(resolveModifierMasks(rows).meta, uint8_t(0x40));
    }

    void noMetaAtAll_isUnmapped()
    {
        ModifierKeysyms rows = defaultKeymap();
        rows[6] = {};
        QCOMPARE(resolveModifierMasks(rows).meta, uint8_t(0));
    }

    void noAltKey_isUnmapped()
    {
        ModifierKeysyms rows = defaultKeymap();
        rows[3] = {};
        const ModifierMasks m = resolveModifierMasks(rows);
        QCOMPARE(m.alt, uint8_t(0));
        QCOMPARE(m.meta, uint8_t(0x40));
    }

    void emptyKeymap_keepsProtocolShiftAndControl()
    {
        const ModifierMasks m = resolveModifierMasks({});
        QCOMPARE(m.shift, uint8_t(0x01));
        QCOMPARE(m.control, uint8_t(0x04));
        QCOMPARE(m.alt, uint8_t(0));
        QCOMPARE(m.meta, uint8_t(0));
    }

    void flags_areIndependent()
    {
        const uint8_t ctrl = 0x04;
        QCOMPARE(modifierFlags(ctrl, 0, 0, 0), int(Inactive));
        QCOMPARE(modifierFlags(ctrl, ctrl, 0, 0), int(Depressed));
        QCOMPARE(modifierFlags(ctrl, 0, ctrl, 0), int(Latched));
        QCOMPARE(modifierFlags(ctrl, 0, 0, ctrl), int(Locked));
        QCOMPARE(modifierFlags(ctrl, ctrl, 0, ctrl), int(Depressed | Locked));
        // Other modifiers' bits do not leak in.
        QCOMPARE(modifierFlags(ctrl, 0x01, 0x08, 0x10), int(Inactive));
    }

    void flags_unmappedModifierIsAlwaysInactive()
    {
        QCOMPARE(modifierFlags(0, 0xff, 0xff, 0xff), int(Inactive));
    }

    void flags_multiBitMask()
    {
        // A modifier bound to two real modifiers is active if either bit is.
        QCOMPARE(modifierFlags(0x48, 0, 0x40, 0), int(Latched));
    }
};

QTEST_GUILESS_MAIN(TestModifierMasks)
#include "test_modifiermasks.moc"
