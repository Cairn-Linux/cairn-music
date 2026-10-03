#include <QtTest>
#include <QtTest/qtestaccessible.h>
#include <QAccessible>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTemporaryDir>

#include "AppController.h"

class QmlSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void loadsPicturesWorkspace();
    void showsAutosaveFailureWarning();
    void showsRecoverableAudioFailureWarning();
};

void QmlSmokeTest::loadsPicturesWorkspace()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    AppController controller(directory.filePath("autosave.json"), false);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));

    QCOMPARE(engine.rootObjects().size(), 1);
    QObject *root = engine.rootObjects().constFirst();
    QVERIFY(root->findChild<QObject *>("pitchGrid") != nullptr);
    QVERIFY(root->findChild<QObject *>("drumLane") != nullptr);
    QVERIFY(root->findChild<QObject *>("playButton") != nullptr);
}

void QmlSmokeTest::showsAutosaveFailureWarning()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString blockedParent = directory.filePath("blocked");
    QFile blocker(blockedParent);
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    QVERIFY(blocker.write("not a directory") > 0);
    blocker.close();

    AppController controller(blockedParent + QStringLiteral("/autosave.json"), false);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);

    controller.selectPitched(0);
    QVERIFY(!controller.placePitched(0, 0));
    QCoreApplication::processEvents();

    QObject *banner = engine.rootObjects().constFirst()->findChild<QObject *>(
        "saveFailureBanner");
    QVERIFY(banner != nullptr);
    QVERIFY(banner->property("visible").toBool());
    QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(banner);
    QVERIFY(accessible != nullptr);
    QCOMPARE(accessible->text(QAccessible::Name), controller.saveFailureMessage());
}

void QmlSmokeTest::showsRecoverableAudioFailureWarning()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<AudioEngine>(false);
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);

    QTestAccessibility::initialize();
    QTestAccessibility::clearEvents();
    controller.play();
    QCoreApplication::processEvents();

    QObject *root = engine.rootObjects().constFirst();
    QObject *banner = root->findChild<QObject *>("audioFailureBanner");
    QVERIFY(banner != nullptr);
    QVERIFY(banner->property("visible").toBool());
    QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(banner);
    QVERIFY(accessible != nullptr);
    QCOMPARE(accessible->text(QAccessible::Name), controller.audioFailureMessage());
    QCOMPARE(accessible->role(), QAccessible::AlertMessage);

    bool announced = false;
    for (const QAccessibleEvent *event : QTestAccessibility::events()) {
        if (event->type() != QAccessible::Announcement) {
            continue;
        }
        const auto *announcement = static_cast<const QAccessibleAnnouncementEvent *>(event);
        if (announcement->message() == controller.audioFailureMessage()
            && announcement->politeness() == QAccessible::AnnouncementPoliteness::Polite) {
            announced = true;
            break;
        }
    }
    QVERIFY(announced);

    QObject *playButton = root->findChild<QObject *>("playButton");
    QVERIFY(playButton != nullptr);
    QVERIFY(playButton->property("text").toString().contains(QStringLiteral("Play")));

    qDeleteAll(QTestAccessibility::events());
    QTestAccessibility::clearEvents();
    QTestAccessibility::cleanup();
}

QTEST_MAIN(QmlSmokeTest)
#include "tst_qmlsmoke.moc"
