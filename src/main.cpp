#include "AppController.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>

int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("Cairn Music Prototype"));
    application.setOrganizationName(QStringLiteral("Cairn Linux"));

    AppController controller;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    const QStringList arguments = application.arguments();
    const int screenshotIndex = arguments.indexOf(QStringLiteral("--screenshot"));
    if (screenshotIndex >= 0 && screenshotIndex + 1 < arguments.size()) {
        const QString path = arguments.at(screenshotIndex + 1);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        if (!window) {
            return 2;
        }
        QTimer::singleShot(500, window, [window, path, &application]() {
            const bool saved = window->grabWindow().save(path);
            application.exit(saved ? 0 : 3);
        });
    }

    return application.exec();
}
