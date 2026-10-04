#include <QtTest>
#include <QtTest/qtestaccessible.h>
#include <QAccessible>
#include <QFile>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>

#include "AppController.h"

namespace {
bool invokePitchedPlacement(QObject *root, int step, int pitch, bool &placed)
{
    QVariant returned;
    const bool invoked = QMetaObject::invokeMethod(
        root, "placePitchedAt", Qt::DirectConnection,
        Q_RETURN_ARG(QVariant, returned),
        Q_ARG(QVariant, QVariant(step)), Q_ARG(QVariant, QVariant(pitch)));
    placed = returned.toBool();
    return invoked;
}

QQuickItem *findQuickItem(QQuickItem *parent, const QString &objectName)
{
    if (!parent) {
        return nullptr;
    }
    if (parent->objectName() == objectName) {
        return parent;
    }
    for (QQuickItem *child : parent->childItems()) {
        if (QQuickItem *match = findQuickItem(child, objectName)) {
            return match;
        }
    }
    return nullptr;
}

bool clickQuickItem(QObject *root, const QString &objectName)
{
    auto *window = qobject_cast<QQuickWindow *>(root);
    auto *item = window ? findQuickItem(window->contentItem(), objectName) : nullptr;
    if (!window || !item || !item->isVisible()) {
        return false;
    }
    const QPoint point = item->mapToScene(
        QPointF(item->width() / 2.0, item->height() / 2.0)).toPoint();
    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point);
    QCoreApplication::processEvents();
    return true;
}
}

class QmlSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void loadsPicturesWorkspace();
    void showsAutosaveFailureWarning();
    void showsRecoverableAudioFailureWarning();
    void clicksPitchedPlacementPaths();
    void handlesPitchedPlacementFeedback();
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

    QObject *root = engine.rootObjects().constFirst();
    QObject *feedback = root->findChild<QObject *>("placementFeedback");
    QVERIFY(feedback != nullptr);

    controller.selectPitched(0);
    bool placed = true;
    QVERIFY(invokePitchedPlacement(root, 0, 0, placed));
    QVERIFY(!placed);
    QCoreApplication::processEvents();

    QObject *banner = root->findChild<QObject *>("saveFailureBanner");
    QVERIFY(banner != nullptr);
    QVERIFY(banner->property("visible").toBool());
    QVERIFY(!feedback->property("visible").toBool());

    for (int pitch = 1; pitch < 3; ++pitch) {
        QVERIFY(invokePitchedPlacement(root, 0, pitch, placed));
        QVERIFY(!placed);
        QVERIFY(!feedback->property("visible").toBool());
    }
    QVERIFY(invokePitchedPlacement(root, 0, 3, placed));
    QVERIFY(!placed);
    QVERIFY(feedback->property("visible").toBool());

    QVERIFY(invokePitchedPlacement(root, 0, 1, placed));
    QVERIFY(!placed);
    QVERIFY(!controller.pitchedPlacementRejected());
    QVERIFY(!feedback->property("visible").toBool());
    QCOMPARE(controller.composition()->rowCount(), 3);

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

void QmlSmokeTest::clicksPitchedPlacementPaths()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    AppController controller(directory.filePath("autosave.json"), false);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);

    QObject *root = engine.rootObjects().constFirst();
    QObject *feedback = root->findChild<QObject *>("placementFeedback");
    QVERIFY(feedback != nullptr);

    controller.selectPitched(0);
    QVERIFY(clickQuickItem(root, QStringLiteral("pitchCellMouse-0-0")));
    QCOMPARE(controller.composition()->rowCount(), 1);

    controller.selectPitched(1);
    QVERIFY(clickQuickItem(root, QStringLiteral("pitchedTokenMouse-0-0")));
    QCOMPARE(controller.composition()->rowCount(), 1);
    QCOMPARE(controller.composition()->data(
                 controller.composition()->index(0), CompositionModel::SoundIdRole).toInt(), 1);
    QVERIFY(!feedback->property("visible").toBool());

    QVERIFY(clickQuickItem(root, QStringLiteral("pitchCellMouse-0-1")));
    QVERIFY(clickQuickItem(root, QStringLiteral("pitchCellMouse-0-2")));
    QCOMPARE(controller.composition()->rowCount(), 3);

    QVERIFY(clickQuickItem(root, QStringLiteral("pitchCellMouse-0-3")));
    QVERIFY(feedback->property("visible").toBool());
    QCOMPARE(controller.composition()->rowCount(), 3);

    controller.selectPitched(2);
    QVERIFY(clickQuickItem(root, QStringLiteral("pitchedTokenMouse-0-1")));
    QVERIFY(!feedback->property("visible").toBool());
    QCOMPARE(controller.composition()->rowCount(), 3);
}

void QmlSmokeTest::handlesPitchedPlacementFeedback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    AppController controller(directory.filePath("autosave.json"), false);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);

    QObject *root = engine.rootObjects().constFirst();
    QObject *feedback = root->findChild<QObject *>("placementFeedback");
    QVERIFY(feedback != nullptr);
    QVERIFY(!feedback->property("visible").toBool());

    bool placed = false;
    for (int pitch = 0; pitch < 3; ++pitch) {
        QVERIFY(invokePitchedPlacement(root, 0, pitch, placed));
        QVERIFY(placed);
    }
    QCOMPARE(controller.composition()->rowCount(), 3);

    QTestAccessibility::initialize();
    QTestAccessibility::clearEvents();
    QVERIFY(invokePitchedPlacement(root, 0, 3, placed));
    QVERIFY(!placed);
    QCoreApplication::processEvents();

    QVERIFY(feedback->property("visible").toBool());
    QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(feedback);
    QVERIFY(accessible != nullptr);
    QCOMPARE(accessible->role(), QAccessible::AlertMessage);
    QCOMPARE(accessible->text(QAccessible::Name),
             QStringLiteral("Only three sounds can play here."));
    QCOMPARE(controller.composition()->rowCount(), 3);

    bool announced = false;
    for (const QAccessibleEvent *event : QTestAccessibility::events()) {
        if (event->type() != QAccessible::Announcement) {
            continue;
        }
        const auto *announcement = static_cast<const QAccessibleAnnouncementEvent *>(event);
        if (announcement->message() == QStringLiteral("Only three sounds can play here.")
            && announcement->politeness() == QAccessible::AnnouncementPoliteness::Polite) {
            announced = true;
            break;
        }
    }
    QVERIFY(announced);

    QVERIFY(invokePitchedPlacement(root, 0, 1, placed));
    QVERIFY(placed);
    QVERIFY(!feedback->property("visible").toBool());
    QCOMPARE(controller.composition()->rowCount(), 3);

    QVERIFY(invokePitchedPlacement(root, 0, 3, placed));
    QVERIFY(!placed);
    QVERIFY(feedback->property("visible").toBool());
    QTest::qWait(1500);
    QVERIFY(feedback->property("visible").toBool());
    QTRY_VERIFY_WITH_TIMEOUT(!feedback->property("visible").toBool(), 1200);

    qDeleteAll(QTestAccessibility::events());
    QTestAccessibility::clearEvents();
    QTestAccessibility::cleanup();
}

QTEST_MAIN(QmlSmokeTest)
#include "tst_qmlsmoke.moc"
