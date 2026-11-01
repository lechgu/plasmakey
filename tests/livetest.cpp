// plasmakey-livetest: end-to-end check against the running X server, through
// the same XkbWatcher code the widget uses.
//
// It CHANGES live keyboard state for a moment (that is the point of the test):
//  - latches/locks Shift, Ctrl, Alt, Meta via XKB requests (like an external tool would);
//  - if xdotool is installed and Sticky Keys is on, taps Shift and Ctrl through
//    XTest so the X server's real Sticky Keys logic latches and locks them.
// It never sends non-modifier keys, and restores the original state at the end.
// Exit codes: 0 = all passed, 1 = failure, 77 = skipped (no X11/XKB).

#include "xkbwatcher.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QProcess>
#include <QStandardPaths>
#include <QTimer>

#include <X11/XKBlib.h>
#undef Status // Xlib macro; clashes with XkbWatcher::Status

#include <csignal>
#include <cstdio>
#include <functional>

using namespace PlasmaKey;

namespace {

int g_passed = 0;
int g_failed = 0;
qint64 g_maxLatencyMs = 0;

const char *flagsText(int flags)
{
    if (flags & Locked)
        return (flags & Depressed) ? "locked+held" : "locked";
    if (flags & Latched)
        return (flags & Depressed) ? "latched+held" : "latched";
    if (flags & Depressed)
        return "held";
    return "off";
}

int flagsOf(const XkbWatcher &w, uint8_t mask)
{
    const XkbModState s = w.modState();
    return modifierFlags(mask, s.base, s.latched, s.locked);
}

// Runs the event loop until pred() is true or the timeout expires.
bool waitFor(XkbWatcher &w, const std::function<bool()> &pred, int timeoutMs, qint64 *elapsedMs = nullptr)
{
    QElapsedTimer clock;
    clock.start();
    if (!pred()) {
        QEventLoop loop;
        QTimer timeout;
        timeout.setSingleShot(true);
        QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
        QObject::connect(&w, &XkbWatcher::modStateChanged, &loop, [&] {
            if (pred())
                loop.quit();
        });
        timeout.start(timeoutMs);
        loop.exec();
    }
    if (elapsedMs)
        *elapsedMs = clock.elapsed();
    return pred();
}

void sleepWithEvents(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

struct Expectation {
    const char *name;
    uint8_t mask;
    int flags;
};

// Checks that every listed modifier reaches exactly the expected flags.
void expect(XkbWatcher &w, const char *step, std::initializer_list<Expectation> wanted)
{
    qint64 ms = 0;
    const bool ok = waitFor(
        w,
        [&] {
            for (const auto &e : wanted) {
                if (flagsOf(w, e.mask) != e.flags)
                    return false;
            }
            return true;
        },
        1000,
        &ms);

    QString detail;
    for (const auto &e : wanted)
        detail += QStringLiteral(" %1=%2").arg(QLatin1String(e.name), QLatin1String(flagsText(flagsOf(w, e.mask))));
    std::printf("  %s  %-48s %4lld ms  [%s ]\n", ok ? "PASS" : "FAIL", step, static_cast<long long>(ms), qPrintable(detail));
    std::fflush(stdout);
    if (ok) {
        ++g_passed;
        g_maxLatencyMs = std::max(g_maxLatencyMs, ms);
    } else {
        ++g_failed;
    }
}

bool xdotool(const QStringList &args)
{
    return QProcess::execute(QStringLiteral("xdotool"), args) == 0;
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    if (qEnvironmentVariable("XDG_SESSION_TYPE") == QLatin1String("wayland")) {
        std::puts("SKIP: Wayland session (X11 only)");
        return 77;
    }

    int event = 0, error = 0, major = XkbMajorVersion, minor = XkbMinorVersion, reason = 0;
    Display *dpy = XkbOpenDisplay(nullptr, &event, &error, &major, &minor, &reason);
    XkbWatcher watcher;
    if (!dpy || watcher.status() != XkbWatcher::Status::Connected) {
        std::printf("SKIP: no X11 display with XKB (%s)\n", qPrintable(watcher.errorString()));
        return 77;
    }

    const ModifierMasks m = watcher.masks();
    const uint8_t touched = m.shift | m.control | m.alt | m.meta;

    XkbStateRec original;
    XkbGetState(dpy, XkbUseCoreKbd, &original);
    if ((original.base_mods | original.latched_mods | original.locked_mods) & touched) {
        std::printf("Shift/Ctrl/Alt/Meta must be inactive before the test (base=0x%02x latched=0x%02x locked=0x%02x).\n"
                    "Release or unlock them and run again.\n",
                    original.base_mods, original.latched_mods, original.locked_mods);
        return 1;
    }

    // Make sure the restore below always runs: hold off Ctrl+C etc. until then.
    sigset_t blocked, previous;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGINT);
    sigaddset(&blocked, SIGTERM);
    sigaddset(&blocked, SIGHUP);
    sigprocmask(SIG_BLOCK, &blocked, &previous);

    const StickyKeysControls controls = watcher.controls();
    std::printf("Masks: Shift=0x%02x Ctrl=0x%02x Alt=0x%02x Meta=0x%02x   Sticky Keys: %s, lock: %s\n", m.shift, m.control, m.alt, m.meta,
                controls.enabled ? "on" : "off", controls.latchToLock ? "on" : "off");

    auto lock = [&](uint8_t mask, bool on) {
        XkbLockModifiers(dpy, XkbUseCoreKbd, mask, on ? mask : 0);
        XFlush(dpy);
    };
    auto latch = [&](uint8_t mask, bool on) {
        XkbLatchModifiers(dpy, XkbUseCoreKbd, mask, on ? mask : 0);
        XFlush(dpy);
    };

    const Expectation off[] = {{"Shift", m.shift, Inactive}, {"Ctrl", m.control, Inactive}, {"Alt", m.alt, Inactive}, {"Meta", m.meta, Inactive}};

    // 1. External latch/lock requests, one modifier at a time.
    std::puts("\n[1] Latch/lock set from outside the widget (XKB requests), each modifier:");
    for (const auto &mod : off) {
        if (!mod.mask) {
            std::printf("  SKIP  %s is not mapped in this keymap\n", mod.name);
            continue;
        }
        const QByteArray n(mod.name);
        lock(mod.mask, true);
        expect(watcher, (n + " lock -> locked").constData(), {{mod.name, mod.mask, Locked}});
        lock(mod.mask, false);
        expect(watcher, (n + " unlock -> off").constData(), {{mod.name, mod.mask, Inactive}});
        latch(mod.mask, true);
        expect(watcher, (n + " latch -> latched").constData(), {{mod.name, mod.mask, Latched}});
        latch(mod.mask, false);
        expect(watcher, (n + " latch cleared -> off").constData(), {{mod.name, mod.mask, Inactive}});
    }

    // 2. Several at once are tracked independently.
    std::puts("\n[2] Simultaneous modifiers:");
    latch(m.control | m.shift, true);
    expect(watcher, "Ctrl+Shift latched", {{"Ctrl", m.control, Latched}, {"Shift", m.shift, Latched}, {"Alt", m.alt, Inactive}});
    lock(m.alt, true);
    expect(watcher, "Alt locked while Ctrl+Shift latched",
           {{"Ctrl", m.control, Latched}, {"Shift", m.shift, Latched}, {"Alt", m.alt, Locked}});
    // (Unlocking one modifier leaves the others alone. Clearing one latch by request
    // would not: Xorg drops all latches then, so that is not tested here.)
    lock(m.alt, false);
    expect(watcher, "Alt unlocked, Ctrl+Shift still latched", {{"Ctrl", m.control, Latched}, {"Shift", m.shift, Latched}, {"Alt", m.alt, Inactive}});
    latch(touched, false);
    lock(touched, false);
    expect(watcher, "all cleared", {off[0], off[1], off[2], off[3]});

    // 3. Real Sticky Keys behavior through XTest key taps.
    const bool haveXdotool = !QStandardPaths::findExecutable(QStringLiteral("xdotool")).isEmpty();
    QStringList heldKeys;
    if (!haveXdotool) {
        std::puts("\n[3] SKIP Sticky Keys key-tap tests: xdotool not installed");
    } else if (!controls.enabled) {
        std::puts("\n[3] SKIP Sticky Keys key-tap tests: Sticky Keys is off");
    } else {
        std::puts("\n[3] Sticky Keys via simulated key taps (X server's own latch/lock logic):");
        const struct {
            const char *name;
            const char *keysym;
            uint8_t mask;
        } keys[] = {{"Shift", "Shift_L", m.shift}, {"Ctrl", "Control_L", m.control}};
        for (const auto &k : keys) {
            const QByteArray n(k.name);
            const QString sym = QString::fromLatin1(k.keysym);
            heldKeys << sym;
            xdotool({QStringLiteral("keydown"), sym});
            expect(watcher, (n + " held down -> held (not latched/locked)").constData(), {{k.name, k.mask, Depressed}});
            xdotool({QStringLiteral("keyup"), sym});
            heldKeys.removeAll(sym);
            expect(watcher, (n + " released -> latched").constData(), {{k.name, k.mask, Latched}});
            if (controls.latchToLock) {
                xdotool({QStringLiteral("key"), sym});
                expect(watcher, (n + " tapped again -> locked").constData(), {{k.name, k.mask, Locked}});
                sleepWithEvents(300);
                expect(watcher, (n + " still locked after 300 ms").constData(), {{k.name, k.mask, Locked}});
                xdotool({QStringLiteral("key"), sym});
                expect(watcher, (n + " tapped third time -> off").constData(), {{k.name, k.mask, Inactive}});
            } else {
                latch(k.mask, false);
                expect(watcher, (n + " latch cleared -> off").constData(), {{k.name, k.mask, Inactive}});
            }
        }
    }

    // Restore: release anything still held, then put latches/locks back.
    for (const QString &sym : std::as_const(heldKeys))
        xdotool({QStringLiteral("keyup"), sym});
    XkbLatchModifiers(dpy, XkbUseCoreKbd, touched, original.latched_mods & touched);
    XkbLockModifiers(dpy, XkbUseCoreKbd, touched, original.locked_mods & touched);
    XSync(dpy, False);
    XkbStateRec after;
    XkbGetState(dpy, XkbUseCoreKbd, &after);
    XCloseDisplay(dpy);
    const bool restored = ((after.latched_mods ^ original.latched_mods) & touched) == 0 && ((after.locked_mods ^ original.locked_mods) & touched) == 0;
    std::printf("\nRestored original state: %s\n", restored ? "yes" : "NO - press and release Shift/Ctrl/Alt/Meta to clear");

    sigprocmask(SIG_SETMASK, &previous, nullptr);

    std::printf("Result: %d passed, %d failed; slowest update %lld ms\n", g_passed, g_failed, static_cast<long long>(g_maxLatencyMs));
    return (g_failed == 0 && restored) ? 0 : 1;
}
