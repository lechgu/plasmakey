#include "xkbwatcher.h"

#include <QSocketNotifier>

#include <xcb/xcb.h>
// xkb.h has a struct member named "explicit", a C++ keyword (same workaround as Qt).
#define explicit dont_use_cxx_explicit
#include <xcb/xkb.h>
#undef explicit

#include <algorithm>
#include <cstdlib>

Q_LOGGING_CATEGORY(PLASMAKEY_XKB, "plasmakey.xkb", QtWarningMsg)

namespace PlasmaKey {

namespace {

struct FreeDeleter {
    void operator()(void *p) const { std::free(p); }
};
template<typename T>
using XcbPtr = std::unique_ptr<T, FreeDeleter>;

constexpr int MaxReconnectDelayMs = 30000;

QString hexMask(uint8_t mask)
{
    return QStringLiteral("0x%1").arg(uint(mask), 2, 16, QLatin1Char('0'));
}

constexpr uint16_t ModifierStateParts =
    XCB_XKB_STATE_PART_MODIFIER_BASE | XCB_XKB_STATE_PART_MODIFIER_LATCH | XCB_XKB_STATE_PART_MODIFIER_LOCK;

} // namespace

XkbWatcher::XkbWatcher(QObject *parent)
    : QObject(parent)
{
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &XkbWatcher::connectToServer);
    connectToServer();
}

XkbWatcher::~XkbWatcher()
{
    disconnectFromServer();
}

std::shared_ptr<XkbWatcher> XkbWatcher::shared()
{
    static std::weak_ptr<XkbWatcher> s_instance;
    auto instance = s_instance.lock();
    if (!instance) {
        instance = std::make_shared<XkbWatcher>();
        s_instance = instance;
    }
    return instance;
}

void XkbWatcher::connectToServer()
{
    disconnectFromServer();

    m_conn = xcb_connect(nullptr, nullptr);
    if (xcb_connection_has_error(m_conn)) {
        const QString display = qEnvironmentVariable("DISPLAY");
        disconnectFromServer();
        setStatus(Status::Disconnected,
                  display.isEmpty() ? QStringLiteral("DISPLAY is not set") : QStringLiteral("Cannot connect to X display %1").arg(display));
        handleConnectionLost();
        return;
    }

    const xcb_query_extension_reply_t *ext = xcb_get_extension_data(m_conn, &xcb_xkb_id);
    XcbPtr<xcb_xkb_use_extension_reply_t> use;
    if (ext && ext->present) {
        use.reset(xcb_xkb_use_extension_reply(m_conn, xcb_xkb_use_extension(m_conn, XCB_XKB_MAJOR_VERSION, XCB_XKB_MINOR_VERSION), nullptr));
    }
    if (!ext || !ext->present || !use || !use->supported) {
        disconnectFromServer();
        setStatus(Status::NoXkb, QStringLiteral("The X server has no usable XKB extension"));
        return;
    }
    m_xkbFirstEvent = ext->first_event;

    // Subscribe before the first read so no change can slip in between.
    if (!selectEvents()) {
        disconnectFromServer();
        setStatus(Status::NoXkb, QStringLiteral("Could not subscribe to XKB events"));
        return;
    }

    m_notifier = new QSocketNotifier(xcb_get_file_descriptor(m_conn), QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &XkbWatcher::processEvents);

    refreshMapping();
    refreshControls();
    refreshState();

    if (xcb_connection_has_error(m_conn)) {
        handleConnectionLost();
        return;
    }

    m_reconnectDelayMs = 1000;
    setStatus(Status::Connected);
    qCDebug(PLASMAKEY_XKB).noquote() << "connected; masks shift/ctrl/alt/meta =" << hexMask(m_masks.shift) << hexMask(m_masks.control)
                                     << hexMask(m_masks.alt) << hexMask(m_masks.meta);

    // Replies above may have pulled events into XCB's queue without making
    // the socket readable again, so drain once now.
    processEvents();
}

void XkbWatcher::disconnectFromServer()
{
    if (m_notifier) {
        // May run inside the notifier's own activated() signal, so defer deletion.
        m_notifier->setEnabled(false);
        m_notifier->deleteLater();
        m_notifier = nullptr;
    }
    if (m_conn) {
        xcb_disconnect(m_conn);
        m_conn = nullptr;
    }
}

void XkbWatcher::handleConnectionLost()
{
    disconnectFromServer();
    if (m_status != Status::Disconnected || m_error.isEmpty())
        setStatus(Status::Disconnected, QStringLiteral("Lost connection to the X server"));
    setState({});
    qCWarning(PLASMAKEY_XKB) << m_error << "- retrying in" << m_reconnectDelayMs << "ms";
    m_reconnectTimer.start(m_reconnectDelayMs);
    m_reconnectDelayMs = std::min(m_reconnectDelayMs * 2, MaxReconnectDelayMs);
}

bool XkbWatcher::selectEvents()
{
    const uint16_t events = XCB_XKB_EVENT_TYPE_STATE_NOTIFY | XCB_XKB_EVENT_TYPE_MAP_NOTIFY | XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY
        | XCB_XKB_EVENT_TYPE_CONTROLS_NOTIFY;
    // Everything except StateNotify is rare, so take all of their details.
    const uint16_t selectAll = XCB_XKB_EVENT_TYPE_MAP_NOTIFY | XCB_XKB_EVENT_TYPE_NEW_KEYBOARD_NOTIFY | XCB_XKB_EVENT_TYPE_CONTROLS_NOTIFY;

    // StateNotify only for modifier changes (not group or pointer buttons).
    xcb_xkb_select_events_details_t details{};
    details.affectState = ModifierStateParts;
    details.stateDetails = ModifierStateParts;

    const auto cookie = xcb_xkb_select_events_aux_checked(m_conn, XCB_XKB_ID_USE_CORE_KBD, events, 0, selectAll, 0, 0, &details);
    XcbPtr<xcb_generic_error_t> error(xcb_request_check(m_conn, cookie));
    if (error) {
        qCWarning(PLASMAKEY_XKB) << "XkbSelectEvents failed, X error" << error->error_code;
        return false;
    }
    return true;
}

void XkbWatcher::processEvents()
{
    if (!m_conn)
        return;
    while (xcb_generic_event_t *event = xcb_poll_for_event(m_conn)) {
        handleEvent(event);
        std::free(event);
        if (!m_conn)
            return;
    }
    if (xcb_connection_has_error(m_conn))
        handleConnectionLost();
}

void XkbWatcher::handleEvent(const void *rawEvent)
{
    const auto *event = static_cast<const xcb_generic_event_t *>(rawEvent);
    const uint8_t type = event->response_type & 0x7f;

    if (type == 0) {
        const auto *error = reinterpret_cast<const xcb_generic_error_t *>(event);
        qCWarning(PLASMAKEY_XKB) << "X error" << error->error_code << "for request" << error->major_code << error->minor_code;
        return;
    }

    if (type == XCB_MAPPING_NOTIFY) { // core keymap/modmap change (e.g. xmodmap)
        refreshMapping();
        return;
    }

    if (type != m_xkbFirstEvent)
        return;

    // All XKB events share the header layout of StateNotify up to xkbType.
    const auto *xkbEvent = reinterpret_cast<const xcb_xkb_state_notify_event_t *>(event);
    switch (xkbEvent->xkbType) {
    case XCB_XKB_STATE_NOTIFY:
        setState({xkbEvent->baseMods, xkbEvent->latchedMods, xkbEvent->lockedMods, xkbEvent->mods});
        break;
    case XCB_XKB_MAP_NOTIFY:
        refreshMapping();
        break;
    case XCB_XKB_NEW_KEYBOARD_NOTIFY:
        refreshMapping();
        refreshControls();
        refreshState();
        break;
    case XCB_XKB_CONTROLS_NOTIFY:
        refreshControls();
        break;
    default:
        break;
    }
}

void XkbWatcher::refreshState()
{
    XcbPtr<xcb_xkb_get_state_reply_t> reply(xcb_xkb_get_state_reply(m_conn, xcb_xkb_get_state(m_conn, XCB_XKB_ID_USE_CORE_KBD), nullptr));
    if (!reply)
        return;
    setState({reply->baseMods, reply->latchedMods, reply->lockedMods, reply->mods});
}

void XkbWatcher::refreshControls()
{
    XcbPtr<xcb_xkb_get_controls_reply_t> reply(
        xcb_xkb_get_controls_reply(m_conn, xcb_xkb_get_controls(m_conn, XCB_XKB_ID_USE_CORE_KBD), nullptr));
    if (!reply)
        return;

    StickyKeysControls controls;
    controls.known = true;
    controls.enabled = reply->enabledControls & XCB_XKB_BOOL_CTRL_STICKY_KEYS;
    controls.latchToLock = reply->accessXOption & XCB_XKB_AX_OPTION_LATCH_TO_LOCK;
    controls.twoKeysOff = reply->accessXOption & XCB_XKB_AX_OPTION_TWO_KEYS;

    if (controls != m_controls) {
        m_controls = controls;
        qCDebug(PLASMAKEY_XKB) << "sticky keys enabled" << controls.enabled << "lock" << controls.latchToLock << "two-keys-off"
                               << controls.twoKeysOff;
        Q_EMIT controlsChanged();
    }
}

void XkbWatcher::refreshMapping()
{
    // The core keyboard + modifier mapping (what `xmodmap -pm` shows) is
    // derived by the server from the active XKB keymap.
    const xcb_setup_t *setup = xcb_get_setup(m_conn);
    const xcb_keycode_t minKeycode = setup->min_keycode;
    const xcb_keycode_t maxKeycode = setup->max_keycode;

    const auto keyboardCookie = xcb_get_keyboard_mapping(m_conn, minKeycode, maxKeycode - minKeycode + 1);
    const auto modifierCookie = xcb_get_modifier_mapping(m_conn);
    XcbPtr<xcb_get_keyboard_mapping_reply_t> keyboard(xcb_get_keyboard_mapping_reply(m_conn, keyboardCookie, nullptr));
    XcbPtr<xcb_get_modifier_mapping_reply_t> modifiers(xcb_get_modifier_mapping_reply(m_conn, modifierCookie, nullptr));
    if (!keyboard || !modifiers)
        return;

    const int symsPerKeycode = keyboard->keysyms_per_keycode;
    const xcb_keysym_t *syms = xcb_get_keyboard_mapping_keysyms(keyboard.get());
    const int symCount = xcb_get_keyboard_mapping_keysyms_length(keyboard.get());
    const xcb_keycode_t *keycodes = xcb_get_modifier_mapping_keycodes(modifiers.get());
    const int keycodesPerModifier = modifiers->keycodes_per_modifier;

    ModifierKeysyms rows;
    for (int mod = 0; mod < RealModifierCount; ++mod) {
        for (int k = 0; k < keycodesPerModifier; ++k) {
            const xcb_keycode_t keycode = keycodes[mod * keycodesPerModifier + k];
            if (keycode == 0 || keycode < minKeycode || keycode > maxKeycode)
                continue;
            const int first = (keycode - minKeycode) * symsPerKeycode;
            for (int level = 0; level < symsPerKeycode && first + level < symCount; ++level) {
                const xcb_keysym_t sym = syms[first + level];
                if (sym != 0 && std::find(rows[mod].begin(), rows[mod].end(), sym) == rows[mod].end())
                    rows[mod].push_back(sym);
            }
        }
    }

    const ModifierMasks masks = resolveModifierMasks(rows);
    m_keysyms = rows;
    if (masks != m_masks) {
        m_masks = masks;
        qCDebug(PLASMAKEY_XKB).noquote() << "modifier masks shift/ctrl/alt/meta =" << hexMask(masks.shift) << hexMask(masks.control)
                                         << hexMask(masks.alt) << hexMask(masks.meta);
        Q_EMIT masksChanged();
    }
}

void XkbWatcher::setState(const XkbModState &state)
{
    if (state == m_state)
        return;
    m_state = state;
    qCDebug(PLASMAKEY_XKB).noquote() << "state base/latched/locked/effective =" << hexMask(state.base) << hexMask(state.latched)
                                     << hexMask(state.locked) << hexMask(state.effective);
    Q_EMIT modStateChanged();
}

void XkbWatcher::setStatus(Status status, const QString &error)
{
    if (status == m_status && error == m_error)
        return;
    m_status = status;
    m_error = error;
    Q_EMIT statusChanged();
}

} // namespace PlasmaKey
