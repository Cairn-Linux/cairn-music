#include <QtTest>
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

QTEST_MAIN(QmlSmokeTest)
#include "tst_qmlsmoke.moc"
