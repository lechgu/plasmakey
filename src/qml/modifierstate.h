// QML-facing view of the XKB modifier state, used by the plasmoid.
//
//   import plasmakey.xkb as Xkb
//   Xkb.ModifierState { id: mods }
//   mods.control & Xkb.ModifierState.Locked
#pragma once

#include "xkbwatcher.h"

#include <QObject>
#include <QtQml/qqmlregistration.h>

#include <memory>

class ModifierState : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    // false on Wayland (or any non-X11 platform): state would be misleading.
    Q_PROPERTY(bool supported READ supported CONSTANT)
    // true while connected to the X server with XKB.
    Q_PROPERTY(bool connected READ connected NOTIFY statusChanged)
    // Human-readable problem description; empty when everything works.
    Q_PROPERTY(QString problem READ problem NOTIFY statusChanged)

    // ModifierState.Flag bits (Depressed | Latched | Locked) for each modifier.
    Q_PROPERTY(int shift READ shift NOTIFY modifiersChanged)
    Q_PROPERTY(int control READ control NOTIFY modifiersChanged)
    Q_PROPERTY(int alt READ alt NOTIFY modifiersChanged)
    Q_PROPERTY(int meta READ meta NOTIFY modifiersChanged)
    // false if the active keymap binds no key to that modifier.
    Q_PROPERTY(bool altMapped READ altMapped NOTIFY modifiersChanged)
    Q_PROPERTY(bool metaMapped READ metaMapped NOTIFY modifiersChanged)

    // Sticky Keys accessibility settings, as the X server currently has them.
    Q_PROPERTY(bool stickyKeysKnown READ stickyKeysKnown NOTIFY controlsChanged)
    Q_PROPERTY(bool stickyKeysEnabled READ stickyKeysEnabled NOTIFY controlsChanged)
    Q_PROPERTY(bool stickyKeysLockEnabled READ stickyKeysLockEnabled NOTIFY controlsChanged)

public:
    enum Flag {
        Inactive = PlasmaKey::Inactive,
        Depressed = PlasmaKey::Depressed,
        Latched = PlasmaKey::Latched,
        Locked = PlasmaKey::Locked,
    };
    Q_ENUM(Flag)

    explicit ModifierState(QObject *parent = nullptr);

    bool supported() const { return m_supported; }
    bool connected() const;
    QString problem() const;

    int shift() const;
    int control() const;
    int alt() const;
    int meta() const;
    bool altMapped() const;
    bool metaMapped() const;

    bool stickyKeysKnown() const;
    bool stickyKeysEnabled() const;
    bool stickyKeysLockEnabled() const;

Q_SIGNALS:
    void statusChanged();
    void modifiersChanged();
    void controlsChanged();

private:
    int flagsFor(uint8_t mask) const;

    bool m_supported = false;
    QString m_unsupportedReason;
    std::shared_ptr<PlasmaKey::XkbWatcher> m_watcher;
};
