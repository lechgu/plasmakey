// Read-only observer of the X server's XKB modifier state.
//
// Opens its own XCB connection (separate from Qt's), subscribes to XKB
// StateNotify / MapNotify / NewKeyboardNotify / ControlsNotify events and
// re-reads state, keymap and Sticky Keys controls when they change.
// It never sends requests that change keyboard state or settings.
#pragma once

#include "modifiermasks.h"

#include <QLoggingCategory>
#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>

Q_DECLARE_LOGGING_CATEGORY(PLASMAKEY_XKB)

struct xcb_connection_t;
class QSocketNotifier;

namespace PlasmaKey {

// XKB modifier state components (real-modifier bit masks).
struct XkbModState {
    uint8_t base = 0; // depressed: keys physically held
    uint8_t latched = 0; // one-shot (Sticky Keys latch)
    uint8_t locked = 0; // persistent (Sticky Keys lock, Caps/Num Lock, ...)
    uint8_t effective = 0; // what applications see

    bool operator==(const XkbModState &) const = default;
};

// The AccessX (accessibility) controls relevant to Sticky Keys.
struct StickyKeysControls {
    bool known = false; // false until read from the server
    bool enabled = false; // "Use sticky keys"
    bool latchToLock = false; // "Lock sticky keys": press twice to lock
    bool twoKeysOff = false; // "Turn off when two keys are pressed simultaneously"

    bool operator==(const StickyKeysControls &) const = default;
};

class XkbWatcher : public QObject
{
    Q_OBJECT

public:
    enum class Status {
        Disconnected, // not connected yet, or connection lost (retrying)
        Connected,
        NoXkb, // X server without a usable XKB extension (no retry)
    };
    Q_ENUM(Status)

    explicit XkbWatcher(QObject *parent = nullptr);
    ~XkbWatcher() override;

    // One watcher (one X connection) per process, shared by every applet instance.
    static std::shared_ptr<XkbWatcher> shared();

    Status status() const { return m_status; }
    QString errorString() const { return m_error; }
    XkbModState modState() const { return m_state; }
    ModifierMasks masks() const { return m_masks; }
    ModifierKeysyms modifierKeysyms() const { return m_keysyms; }
    StickyKeysControls controls() const { return m_controls; }

Q_SIGNALS:
    void statusChanged();
    void modStateChanged();
    void masksChanged();
    void controlsChanged();

private:
    void connectToServer();
    void disconnectFromServer();
    void handleConnectionLost();
    void processEvents();
    void handleEvent(const void *event); // xcb_generic_event_t (an anonymous struct in xcb.h)
    bool selectEvents();
    void refreshState();
    void refreshControls();
    void refreshMapping();
    void setState(const XkbModState &state);
    void setStatus(Status status, const QString &error = {});

    xcb_connection_t *m_conn = nullptr;
    QSocketNotifier *m_notifier = nullptr;
    QTimer m_reconnectTimer;
    int m_reconnectDelayMs = 1000;
    uint8_t m_xkbFirstEvent = 0;

    Status m_status = Status::Disconnected;
    QString m_error;
    XkbModState m_state;
    ModifierKeysyms m_keysyms;
    ModifierMasks m_masks;
    StickyKeysControls m_controls;
};

} // namespace PlasmaKey
