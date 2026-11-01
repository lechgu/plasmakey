// plasmakey-diag: prints what the indicator sees (XKB masks, modifier mapping,
// Sticky Keys settings) and, unless --once is given, every state change.
// Read-only, like the widget. Not used in normal operation.

#include "xkbwatcher.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QTextStream>

#include <cstdio>

using namespace PlasmaKey;

namespace {

QTextStream out(stdout);

QString hex(uint8_t mask)
{
    return QStringLiteral("0x%1").arg(uint(mask), 2, 16, QLatin1Char('0'));
}

QString maskNames(uint8_t mask)
{
    if (mask == 0)
        return QStringLiteral("(none)");
    QStringList names;
    for (int i = 0; i < RealModifierCount; ++i) {
        if (mask & (1u << i))
            names << QString::fromLatin1(realModifierName(i));
    }
    return names.join(QLatin1Char('+'));
}

QString describe(int flags)
{
    if (flags & Locked)
        return QStringLiteral("LOCKED");
    if (flags & Latched)
        return QStringLiteral("LATCHED");
    if (flags & Depressed)
        return QStringLiteral("held");
    return QStringLiteral("-");
}

void printMapping(const XkbWatcher &w)
{
    out << "Modifier map (real modifier: keysyms bound to it):\n";
    const ModifierKeysyms rows = w.modifierKeysyms();
    for (int i = 0; i < RealModifierCount; ++i) {
        QStringList syms;
        for (uint32_t sym : rows[i]) {
            const char *name = keysymName(sym);
            syms << (name ? QString::fromLatin1(name) : QStringLiteral("0x%1").arg(sym, 0, 16));
        }
        out << QStringLiteral("  %1 %2  %3\n").arg(QString::fromLatin1(realModifierName(i)), -8).arg(hex(uint8_t(1u << i))).arg(syms.join(QLatin1Char(' ')));
    }
    const ModifierMasks m = w.masks();
    out << "Indicator mapping:\n";
    out << QStringLiteral("  Shift = %1 %2\n").arg(hex(m.shift), maskNames(m.shift));
    out << QStringLiteral("  Ctrl  = %1 %2\n").arg(hex(m.control), maskNames(m.control));
    out << QStringLiteral("  Alt   = %1 %2\n").arg(hex(m.alt), maskNames(m.alt));
    out << QStringLiteral("  Meta  = %1 %2\n").arg(hex(m.meta), maskNames(m.meta));
    out.flush();
}

void printControls(const XkbWatcher &w)
{
    const StickyKeysControls c = w.controls();
    auto onOff = [](bool b) { return b ? QStringLiteral("on") : QStringLiteral("off"); };
    out << "Sticky Keys: " << onOff(c.enabled) << "   Lock sticky keys (press twice): " << onOff(c.latchToLock)
        << "   Turn off when two keys pressed: " << onOff(c.twoKeysOff) << "\n";
    out.flush();
}

void printState(const XkbWatcher &w)
{
    const XkbModState s = w.modState();
    const ModifierMasks m = w.masks();
    auto f = [&](uint8_t mask) { return describe(modifierFlags(mask, s.base, s.latched, s.locked)); };
    out << QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"))
        << QStringLiteral("  base=%1 latched=%2 locked=%3 effective=%4  |  Shift:%5 Ctrl:%6 Alt:%7 Meta:%8\n")
               .arg(hex(s.base), hex(s.latched), hex(s.locked), hex(s.effective))
               .arg(f(m.shift), -7)
               .arg(f(m.control), -7)
               .arg(f(m.alt), -7)
               .arg(f(m.meta));
    out.flush();
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QStringList args = app.arguments();
    if (args.contains(QStringLiteral("--help")) || args.contains(QStringLiteral("-h"))) {
        out << "Usage: plasmakey-diag [--once]\n"
               "Prints the XKB modifier masks and Sticky Keys settings the indicator uses,\n"
               "then every modifier state change (base = held, latched, locked).\n"
               "  --once   print the current state and exit\n";
        return 0;
    }
    const bool once = args.contains(QStringLiteral("--once"));

    out << "Session type: " << qEnvironmentVariable("XDG_SESSION_TYPE", QStringLiteral("(unset)")) << "   DISPLAY="
        << qEnvironmentVariable("DISPLAY", QStringLiteral("(unset)")) << "\n";
    if (qEnvironmentVariable("XDG_SESSION_TYPE") == QLatin1String("wayland"))
        out << "WARNING: Wayland session. XKB state seen through Xwayland does not reflect the real keyboard; the widget reports 'not supported'.\n";
    out.flush();

    XkbWatcher watcher;
    if (watcher.status() != XkbWatcher::Status::Connected) {
        std::fprintf(stderr, "plasmakey-diag: %s\n", qPrintable(watcher.errorString()));
        return 1;
    }

    printControls(watcher);
    printMapping(watcher);
    printState(watcher);
    if (once)
        return 0;

    out << "Watching for changes (Ctrl+C to quit; with Sticky Keys on, Ctrl latches first)...\n";
    out.flush();
    QObject::connect(&watcher, &XkbWatcher::modStateChanged, [&] { printState(watcher); });
    QObject::connect(&watcher, &XkbWatcher::controlsChanged, [&] { printControls(watcher); });
    QObject::connect(&watcher, &XkbWatcher::masksChanged, [&] { printMapping(watcher); });
    QObject::connect(&watcher, &XkbWatcher::statusChanged, [&] {
        out << "Status: " << (watcher.status() == XkbWatcher::Status::Connected ? QStringLiteral("connected") : watcher.errorString())
            << "\n";
        out.flush();
    });
    return app.exec();
}
