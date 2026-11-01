#include "modifierstate.h"

#include <QGuiApplication>

using PlasmaKey::XkbWatcher;

ModifierState::ModifierState(QObject *parent)
    : QObject(parent)
{
    // Under Wayland, plasmashell may still reach Xwayland via $DISPLAY, but
    // Xwayland's XKB state is not the real keyboard state. Only trust X11.
    m_supported = QGuiApplication::platformName() == QLatin1String("xcb");
    if (!m_supported) {
        const QString session = qEnvironmentVariable("XDG_SESSION_TYPE", QGuiApplication::platformName());
        m_unsupportedReason = QStringLiteral("Not supported in this session (%1). plasmakey works on X11 only.").arg(session);
        return;
    }

    m_watcher = XkbWatcher::shared();
    connect(m_watcher.get(), &XkbWatcher::statusChanged, this, &ModifierState::statusChanged);
    connect(m_watcher.get(), &XkbWatcher::statusChanged, this, &ModifierState::modifiersChanged);
    connect(m_watcher.get(), &XkbWatcher::modStateChanged, this, &ModifierState::modifiersChanged);
    connect(m_watcher.get(), &XkbWatcher::masksChanged, this, &ModifierState::modifiersChanged);
    connect(m_watcher.get(), &XkbWatcher::controlsChanged, this, &ModifierState::controlsChanged);
}

bool ModifierState::connected() const
{
    return m_watcher && m_watcher->status() == XkbWatcher::Status::Connected;
}

QString ModifierState::problem() const
{
    if (!m_supported)
        return m_unsupportedReason;
    if (connected())
        return {};
    if (m_watcher->status() == XkbWatcher::Status::NoXkb)
        return m_watcher->errorString();
    return m_watcher->errorString().isEmpty() ? QStringLiteral("Connecting to the X server…")
                                              : m_watcher->errorString() + QStringLiteral("; retrying…");
}

int ModifierState::flagsFor(uint8_t mask) const
{
    if (!connected())
        return Inactive;
    const PlasmaKey::XkbModState s = m_watcher->modState();
    return PlasmaKey::modifierFlags(mask, s.base, s.latched, s.locked);
}

int ModifierState::shift() const
{
    return m_watcher ? flagsFor(m_watcher->masks().shift) : Inactive;
}

int ModifierState::control() const
{
    return m_watcher ? flagsFor(m_watcher->masks().control) : Inactive;
}

int ModifierState::alt() const
{
    return m_watcher ? flagsFor(m_watcher->masks().alt) : Inactive;
}

int ModifierState::meta() const
{
    return m_watcher ? flagsFor(m_watcher->masks().meta) : Inactive;
}

bool ModifierState::altMapped() const
{
    return m_watcher && m_watcher->masks().alt != 0;
}

bool ModifierState::metaMapped() const
{
    return m_watcher && m_watcher->masks().meta != 0;
}

bool ModifierState::stickyKeysKnown() const
{
    return m_watcher && m_watcher->controls().known;
}

bool ModifierState::stickyKeysEnabled() const
{
    return m_watcher && m_watcher->controls().enabled;
}

bool ModifierState::stickyKeysLockEnabled() const
{
    return m_watcher && m_watcher->controls().latchToLock;
}
