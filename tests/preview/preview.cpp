// plasmakey-preview: renders the plasmoid's QML (icon in every state, light and
// dark, plus the popup) offscreen into a PNG, without touching the desktop.
// Usage: plasmakey-preview OUTPUT.png   (run with QT_QPA_PLATFORM=offscreen)

#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include <QUrl>

#include <cstdio>

// Stand-in for KLocalizedContext (which plasmashell provides): i18n(), i18nc().
class I18nContext : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE QString i18n(const QString &text, const QVariant &a1 = {}, const QVariant &a2 = {}) const { return subst(text, a1, a2); }
    Q_INVOKABLE QString i18nc(const QString &, const QString &text, const QVariant &a1 = {}, const QVariant &a2 = {}) const
    {
        return subst(text, a1, a2);
    }

private:
    static QString subst(QString text, const QVariant &a1, const QVariant &a2)
    {
        if (a1.isValid())
            text.replace(QLatin1String("%1"), a1.toString());
        if (a2.isValid())
            text.replace(QLatin1String("%2"), a2.toString());
        return text;
    }
};

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    if (argc < 2) {
        std::fprintf(stderr, "usage: plasmakey-preview OUTPUT.png\n");
        return 2;
    }

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral(PLASMAKEY_BUILD_QML_DIR));
    I18nContext i18n;
    engine.rootContext()->setContextObject(&i18n);
    engine.rootContext()->setContextProperty(QStringLiteral("outputPath"), QDir::current().absoluteFilePath(QString::fromLocal8Bit(argv[1])));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.load(QUrl::fromLocalFile(QStringLiteral(PLASMAKEY_PREVIEW_QML)));
    QTimer::singleShot(20000, &app, [] {
        std::fprintf(stderr, "plasmakey-preview: timed out\n");
        QCoreApplication::exit(1);
    });
    return app.exec();
}

#include "preview.moc"
