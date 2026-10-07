#include <QtTest>
#include <QtTest/qtestaccessible.h>
#include <QAccessible>
#include <QFile>
#include <QFileInfo>
#include <QFont>
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

QQuickItem *findQuickItemByText(QQuickItem *parent, const QString &text)
{
    if (!parent) {
        return nullptr;
    }
    if (parent->property("text").toString() == text) {
        return parent;
    }
    for (QQuickItem *child : parent->childItems()) {
        if (QQuickItem *match = findQuickItemByText(child, text)) {
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

class QmlAudioEngine final : public AudioEngine
{
public:
    QmlAudioEngine()
        : AudioEngine(false)
    {
    }

    void finishCurrentBuffer()
    {
        m_outputState = QAudio::IdleState;
        handleState(m_outputState);
    }

protected:
    [[nodiscard]] bool outputAvailable() const noexcept override { return true; }
    [[nodiscard]] QAudio::Error outputError() const noexcept override
    {
        return m_outputError;
    }
    [[nodiscard]] QAudio::State outputState() const noexcept override
    {
        return m_outputState;
    }
    void startOutput(QIODevice *) override
    {
        m_outputError = QAudio::NoError;
        m_outputState = QAudio::ActiveState;
    }
    void stopOutput() override
    {
        m_outputError = QAudio::NoError;
        m_outputState = QAudio::StoppedState;
        handleState(m_outputState);
    }

private:
    QAudio::Error m_outputError = QAudio::NoError;
    QAudio::State m_outputState = QAudio::StoppedState;
};
}

class QmlSmokeTest : public QObject
{
    Q_OBJECT

private slots:
    void loadsPicturesWorkspace();
    void showsAutosaveFailureWarning();
    void showsRecoverableAudioFailureWarning();
    void selectsEverySoundThroughPointerPath();
    void placesPercussionThroughSeparateLane();
    void erasesAndUndoesMultiplePointerEdits();
    void addsMeasuresThroughPrototypeLimit();
    void controlsPlayStopAndLoopThroughQml();
    void reopensPointerEditsFromTemporaryAutosave();
    void exposesKeyboardFocusAndAccessibleControlNames();
    void keepsSoundLabelsReadableAtSupportedWindowSizes();
    void clicksPitchedPlacementPaths();
    void handlesPitchedPlacementFeedback();
    void keepsAddMeasureUsableAtMinimumSize();
    void showsCurrentPlaybackStepAndSoundingEvents();
    void makesLoopRestartVisibleAndAnnouncesIt();
    void clearsLoopRestartNoticeWhenLoopIsToggled();
    void clearsLoopRestartNoticeForStopAndReplay();
    void clearsLoopRestartNoticeForSoundPreview();
    void clearsPlaybackIndicatorsForUndoMutation();
    void playheadMovesWithScrollableTimeline();
    void showsSelectedToolBesidePointerOnlyOverGrid();
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

void QmlSmokeTest::selectsEverySoundThroughPointerPath()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    AppController controller(directory.filePath("autosave.json"), false);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);
    QObject *root = engine.rootObjects().constFirst();
    auto *window = qobject_cast<QQuickWindow *>(root);
    QVERIFY(window != nullptr);

    for (int sound = 0; sound < 4; ++sound) {
        const QString objectName = QStringLiteral("pitchedSoundButton-%1").arg(sound);
        QQuickItem *button = findQuickItem(window->contentItem(), objectName);
        QVERIFY2(button != nullptr, qPrintable(objectName));
        QVERIFY(clickQuickItem(root, objectName));
        QCOMPARE(controller.selectedKind(), QStringLiteral("pitched"));
        QCOMPARE(controller.selectedSound(), sound);
        for (int candidate = 0; candidate < 4; ++candidate) {
            QQuickItem *candidateButton = findQuickItem(
                window->contentItem(),
                QStringLiteral("pitchedSoundButton-%1").arg(candidate));
            QVERIFY(candidateButton != nullptr);
            QCOMPARE(candidateButton->property("checked").toBool(), candidate == sound);
        }
        for (int candidate = 0; candidate < 2; ++candidate) {
            QQuickItem *candidateButton = findQuickItem(
                window->contentItem(),
                QStringLiteral("percussionSoundButton-%1").arg(candidate));
            QVERIFY(candidateButton != nullptr);
            QVERIFY(!candidateButton->property("checked").toBool());
        }
    }

    for (int sound = 0; sound < 2; ++sound) {
        const QString objectName = QStringLiteral("percussionSoundButton-%1").arg(sound);
        QQuickItem *button = findQuickItem(window->contentItem(), objectName);
        QVERIFY2(button != nullptr, qPrintable(objectName));
        QVERIFY(clickQuickItem(root, objectName));
        QCOMPARE(controller.selectedKind(), QStringLiteral("percussion"));
        QCOMPARE(controller.selectedSound(), sound);
        for (int candidate = 0; candidate < 4; ++candidate) {
            QQuickItem *candidateButton = findQuickItem(
                window->contentItem(),
                QStringLiteral("pitchedSoundButton-%1").arg(candidate));
            QVERIFY(candidateButton != nullptr);
            QVERIFY(!candidateButton->property("checked").toBool());
        }
        for (int candidate = 0; candidate < 2; ++candidate) {
            QQuickItem *candidateButton = findQuickItem(
                window->contentItem(),
                QStringLiteral("percussionSoundButton-%1").arg(candidate));
            QVERIFY(candidateButton != nullptr);
            QCOMPARE(candidateButton->property("checked").toBool(), candidate == sound);
        }
    }
}

void QmlSmokeTest::placesPercussionThroughSeparateLane()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    AppController controller(directory.filePath("autosave.json"), false);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);
    QObject *root = engine.rootObjects().constFirst();
    auto *window = qobject_cast<QQuickWindow *>(root);
    QVERIFY(window != nullptr);
    window->resize(window->width(), 1000);
    QCoreApplication::processEvents();

    QVERIFY(clickQuickItem(root, QStringLiteral("percussionSoundButton-1")));
    QVERIFY(clickQuickItem(root, QStringLiteral("drumCellMouse-0-0")));
    QCOMPARE(controller.composition()->rowCount(), 0);

    QVERIFY(clickQuickItem(root, QStringLiteral("drumCellMouse-0-1")));
    QCOMPARE(controller.composition()->rowCount(), 1);
    const QModelIndex token = controller.composition()->index(0);
    QCOMPARE(controller.composition()->data(token, CompositionModel::KindRole).toString(),
             QStringLiteral("percussion"));
    QCOMPARE(controller.composition()->data(token, CompositionModel::StepRole).toInt(), 0);
    QCOMPARE(controller.composition()->data(token, CompositionModel::PitchRowRole).toInt(), 1);
    QCOMPARE(controller.composition()->data(token, CompositionModel::SoundIdRole).toInt(), 1);
}

void QmlSmokeTest::erasesAndUndoesMultiplePointerEdits()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    AppController controller(directory.filePath("autosave.json"), false);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);
    QObject *root = engine.rootObjects().constFirst();

    QVERIFY(clickQuickItem(root, QStringLiteral("pitchCellMouse-0-0")));
    QVERIFY(clickQuickItem(root, QStringLiteral("pitchCellMouse-1-1")));
    QCOMPARE(controller.composition()->rowCount(), 2);

    QVERIFY(clickQuickItem(root, QStringLiteral("eraserButton")));
    QVERIFY(root->property("eraseMode").toBool());
    QVERIFY(clickQuickItem(root, QStringLiteral("pitchedTokenMouse-1-1")));
    QCOMPARE(controller.composition()->rowCount(), 1);

    QVERIFY(clickQuickItem(root, QStringLiteral("undoButton")));
    QCOMPARE(controller.composition()->rowCount(), 2);
    QVERIFY(clickQuickItem(root, QStringLiteral("undoButton")));
    QCOMPARE(controller.composition()->rowCount(), 1);
    const QModelIndex remaining = controller.composition()->index(0);
    QCOMPARE(controller.composition()->data(
                 remaining, CompositionModel::StepRole).toInt(), 0);
    QCOMPARE(controller.composition()->data(
                 remaining, CompositionModel::PitchRowRole).toInt(), 0);
}

void QmlSmokeTest::addsMeasuresThroughPrototypeLimit()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    AppController controller(directory.filePath("autosave.json"), false);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);
    QObject *root = engine.rootObjects().constFirst();
    auto *window = qobject_cast<QQuickWindow *>(root);
    QVERIFY(window != nullptr);
    QQuickItem *button = findQuickItem(window->contentItem(), QStringLiteral("addMeasureButton"));
    QVERIFY(button != nullptr);

    QCOMPARE(controller.composition()->measureCount(), 2);
    for (int expected = 3; expected <= 8; ++expected) {
        QVERIFY(clickQuickItem(root, QStringLiteral("addMeasureButton")));
        QCOMPARE(controller.composition()->measureCount(), expected);
    }
    QVERIFY(!button->property("enabled").toBool());
    QVERIFY(clickQuickItem(root, QStringLiteral("addMeasureButton")));
    QCOMPARE(controller.composition()->measureCount(), 8);
}

void QmlSmokeTest::controlsPlayStopAndLoopThroughQml()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<QmlAudioEngine>();
    QmlAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true, std::move(audio));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);
    QObject *root = engine.rootObjects().constFirst();

    QVERIFY(!controller.loopEnabled());
    QVERIFY(clickQuickItem(root, QStringLiteral("loopCheckBox")));
    QVERIFY(controller.loopEnabled());

    QVERIFY(clickQuickItem(root, QStringLiteral("playButton")));
    QVERIFY(controller.playing());
    QVERIFY(fakeAudio->loopEnabled());
    QObject *playButton = root->findChild<QObject *>("playButton");
    QVERIFY(playButton != nullptr);
    QVERIFY(playButton->property("text").toString().contains(QStringLiteral("Stop")));

    QVERIFY(clickQuickItem(root, QStringLiteral("loopCheckBox")));
    QVERIFY(!controller.loopEnabled());
    QVERIFY(!fakeAudio->loopEnabled());

    QVERIFY(clickQuickItem(root, QStringLiteral("playButton")));
    QVERIFY(!controller.playing());
    QVERIFY(playButton->property("text").toString().contains(QStringLiteral("Play")));
}

void QmlSmokeTest::reopensPointerEditsFromTemporaryAutosave()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString autosavePath = directory.filePath("autosave.json");

    {
        AppController controller(autosavePath, false);
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("app", &controller);
        engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
        QCOMPARE(engine.rootObjects().size(), 1);
        QObject *root = engine.rootObjects().constFirst();

        QVERIFY(clickQuickItem(root, QStringLiteral("pitchedSoundButton-2")));
        QVERIFY(clickQuickItem(root, QStringLiteral("pitchCellMouse-2-4")));
        QVERIFY(clickQuickItem(root, QStringLiteral("addMeasureButton")));
        QCOMPARE(controller.composition()->rowCount(), 1);
        QCOMPARE(controller.composition()->measureCount(), 3);
    }

    QVERIFY(QFileInfo::exists(autosavePath));
    AppController reopened(autosavePath, false);
    QCOMPARE(reopened.composition()->rowCount(), 1);
    QCOMPARE(reopened.composition()->measureCount(), 3);
    const QModelIndex token = reopened.composition()->index(0);
    QCOMPARE(reopened.composition()->data(token, CompositionModel::StepRole).toInt(), 2);
    QCOMPARE(reopened.composition()->data(token, CompositionModel::PitchRowRole).toInt(), 4);
    QCOMPARE(reopened.composition()->data(token, CompositionModel::SoundIdRole).toInt(), 2);

    QQmlApplicationEngine reopenedEngine;
    reopenedEngine.rootContext()->setContextProperty("app", &reopened);
    reopenedEngine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(reopenedEngine.rootObjects().size(), 1);
    auto *window = qobject_cast<QQuickWindow *>(reopenedEngine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    QVERIFY(findQuickItem(window->contentItem(),
                          QStringLiteral("pitchedTokenMouse-2-4")) != nullptr);
}

void QmlSmokeTest::exposesKeyboardFocusAndAccessibleControlNames()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    AppController controller(directory.filePath("autosave.json"), false);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);
    QObject *root = engine.rootObjects().constFirst();
    auto *window = qobject_cast<QQuickWindow *>(root);
    QVERIFY(window != nullptr);

    struct ControlExpectation {
        QString objectName;
        QString accessibleName;
        QAccessible::Role role;
    };
    const QList<ControlExpectation> controls = {
        {QStringLiteral("undoButton"), QStringLiteral("Undo last change"),
         QAccessible::Button},
        {QStringLiteral("eraserButton"), QStringLiteral("Eraser tool"),
         QAccessible::CheckBox},
        {QStringLiteral("playButton"), QStringLiteral("Play song"),
         QAccessible::Button},
        {QStringLiteral("loopCheckBox"), QStringLiteral("Loop whole song"),
         QAccessible::CheckBox},
        {QStringLiteral("addMeasureButton"), QStringLiteral("Add one measure"),
         QAccessible::Button},
        {QStringLiteral("pitchedSoundButton-0"), QStringLiteral("Keys"),
         QAccessible::CheckBox},
        {QStringLiteral("pitchedSoundButton-1"), QStringLiteral("Bell"),
         QAccessible::CheckBox},
        {QStringLiteral("pitchedSoundButton-2"), QStringLiteral("Bird"),
         QAccessible::CheckBox},
        {QStringLiteral("pitchedSoundButton-3"), QStringLiteral("Bubble"),
         QAccessible::CheckBox},
        {QStringLiteral("percussionSoundButton-0"), QStringLiteral("Thump"),
         QAccessible::CheckBox},
        {QStringLiteral("percussionSoundButton-1"), QStringLiteral("Clap"),
         QAccessible::CheckBox},
    };

    for (const ControlExpectation &expectation : controls) {
        QQuickItem *control = findQuickItem(window->contentItem(), expectation.objectName);
        QVERIFY2(control != nullptr, qPrintable(expectation.objectName));
        QVERIFY2(control->property("activeFocusOnTab").toBool(),
                 qPrintable(expectation.objectName));
        QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(control);
        QVERIFY2(accessible != nullptr, qPrintable(expectation.objectName));
        QCOMPARE(accessible->text(QAccessible::Name), expectation.accessibleName);
        QCOMPARE(accessible->role(), expectation.role);
    }

    QQuickItem *bubbleButton = findQuickItem(
        window->contentItem(), QStringLiteral("pitchedSoundButton-3"));
    QVERIFY(bubbleButton != nullptr);
    bubbleButton->forceActiveFocus(Qt::TabFocusReason);
    QVERIFY(bubbleButton->hasActiveFocus());
    QTest::keyClick(window, Qt::Key_Space);
    QCoreApplication::processEvents();
    QCOMPARE(controller.selectedKind(), QStringLiteral("pitched"));
    QCOMPARE(controller.selectedSound(), 3);
}

void QmlSmokeTest::keepsSoundLabelsReadableAtSupportedWindowSizes()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    AppController controller(directory.filePath("autosave.json"), false);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);

    struct LabelExpectation {
        QString buttonName;
        QString text;
    };
    const QList<LabelExpectation> labels = {
        {QStringLiteral("pitchedSoundButton-0"), QStringLiteral("Keys")},
        {QStringLiteral("pitchedSoundButton-1"), QStringLiteral("Bell")},
        {QStringLiteral("pitchedSoundButton-2"), QStringLiteral("Bird")},
        {QStringLiteral("pitchedSoundButton-3"), QStringLiteral("Bubble")},
        {QStringLiteral("percussionSoundButton-0"), QStringLiteral("Thump")},
        {QStringLiteral("percussionSoundButton-1"), QStringLiteral("Clap")},
    };
    const QList<QSize> supportedSizes = {QSize(1180, 760), QSize(900, 620)};

    for (const QSize &size : supportedSizes) {
        window->resize(size);
        QCoreApplication::processEvents();
        QCOMPARE(window->size(), size);

        for (const LabelExpectation &expectation : labels) {
            QQuickItem *button = findQuickItem(window->contentItem(), expectation.buttonName);
            QVERIFY2(button != nullptr, qPrintable(expectation.buttonName));
            QQuickItem *label = findQuickItemByText(button, expectation.text);
            QVERIFY2(label != nullptr, qPrintable(expectation.text));
            QVERIFY2(label->isVisible(), qPrintable(expectation.text));

            const QFont font = label->property("font").value<QFont>();
            QVERIFY2(font.pixelSize() >= 18, qPrintable(expectation.text));

            const QRectF labelRect = label->mapRectToItem(
                button, QRectF(0, 0, label->width(), label->height()));
            QVERIFY2(QRectF(0, 0, button->width(), button->height()).contains(labelRect),
                     qPrintable(expectation.text));
            QVERIFY2(button->width() >= 44 && button->height() >= 44,
                     qPrintable(expectation.buttonName));
        }
    }
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

void QmlSmokeTest::keepsAddMeasureUsableAtMinimumSize()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    AppController controller(directory.filePath("autosave.json"), false);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    window->resize(900, 620);
    QCoreApplication::processEvents();
    QCOMPARE(window->size(), QSize(900, 620));

    QQuickItem *button = findQuickItem(window->contentItem(),
                                       QStringLiteral("addMeasureButton"));
    QVERIFY(button != nullptr);
    const QRectF buttonRect = button->mapRectToScene(button->boundingRect());
    const QRectF windowRect(QPointF(0, 0), window->size());
    QVERIFY2(windowRect.contains(buttonRect),
             qPrintable(QStringLiteral("Add measure bounds %1,%2 %3x%4 exceed window %5x%6")
                            .arg(buttonRect.x()).arg(buttonRect.y())
                            .arg(buttonRect.width()).arg(buttonRect.height())
                            .arg(window->width()).arg(window->height())));
    QVERIFY(buttonRect.width() >= 44.0);
    QVERIFY(buttonRect.height() >= 44.0);

    QCOMPARE(controller.composition()->measureCount(), 2);
    QVERIFY(clickQuickItem(window, QStringLiteral("addMeasureButton")));
    QCOMPARE(controller.composition()->measureCount(), 3);
}

void QmlSmokeTest::showsCurrentPlaybackStepAndSoundingEvents()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<QmlAudioEngine>();
    AppController controller(directory.filePath("autosave.json"), true, std::move(audio));
    controller.selectPitched(1);
    QVERIFY(controller.placePitched(0, 3));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);
    QObject *root = engine.rootObjects().constFirst();

    auto *window = qobject_cast<QQuickWindow *>(root);
    QVERIFY(window != nullptr);
    QQuickItem *playhead = findQuickItem(window->contentItem(), "playbackPlayhead");
    QObject *status = root->findChild<QObject *>("playbackStatus");
    QQuickItem *token = findQuickItem(window->contentItem(), "compositionToken-0-3");
    QVERIFY(playhead != nullptr);
    QVERIFY(status != nullptr);
    QVERIFY(token != nullptr);
    QVERIFY(!playhead->property("visible").toBool());
    QVERIFY(!playhead->property("enabled").toBool());

    controller.play();
    QCoreApplication::processEvents();
    QVERIFY(playhead->property("visible").toBool());
    QCOMPARE(playhead->property("currentStep").toInt(), 0);
    QVERIFY(token->property("sounding").toBool());
    QCOMPARE(status->property("text").toString(), QStringLiteral("Playing beat 1 of 8."));

    QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(status);
    QVERIFY(accessible != nullptr);
    QCOMPARE(accessible->text(QAccessible::Name), controller.playbackStatus());
    QCOMPARE(accessible->role(), QAccessible::StaticText);

    QTRY_COMPARE_WITH_TIMEOUT(playhead->property("currentStep").toInt(), 1, 900);
    QVERIFY(!token->property("sounding").toBool());
}

void QmlSmokeTest::makesLoopRestartVisibleAndAnnouncesIt()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<QmlAudioEngine>();
    QmlAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true, std::move(audio));
    controller.setLoopEnabled(true);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);
    QObject *root = engine.rootObjects().constFirst();
    QObject *restartBadge = root->findChild<QObject *>("loopRestartBadge");
    QVERIFY(restartBadge != nullptr);
    QVERIFY(!restartBadge->property("visible").toBool());

    QTestAccessibility::initialize();
    QTestAccessibility::clearEvents();
    controller.play();
    fakeAudio->finishCurrentBuffer();
    QCoreApplication::processEvents();

    QVERIFY(restartBadge->property("visible").toBool());
    QCOMPARE(restartBadge->property("text").toString(),
             QStringLiteral("Loop 2 • back to beat 1"));

    bool announced = false;
    for (const QAccessibleEvent *event : QTestAccessibility::events()) {
        if (event->type() != QAccessible::Announcement) {
            continue;
        }
        const auto *announcement = static_cast<const QAccessibleAnnouncementEvent *>(event);
        if (announcement->message() == controller.playbackStatus()) {
            announced = true;
            break;
        }
    }
    QVERIFY(announced);
    QTRY_VERIFY_WITH_TIMEOUT(!restartBadge->property("visible").toBool(), 2200);

    qDeleteAll(QTestAccessibility::events());
    QTestAccessibility::clearEvents();
    QTestAccessibility::cleanup();
}

void QmlSmokeTest::clearsLoopRestartNoticeWhenLoopIsToggled()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<QmlAudioEngine>();
    QmlAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true, std::move(audio));
    controller.setLoopEnabled(true);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);
    QObject *root = engine.rootObjects().constFirst();
    QObject *restartBadge = root->findChild<QObject *>("loopRestartBadge");
    QVERIFY(restartBadge != nullptr);
    QObject *restartTimer = nullptr;
    for (QObject *candidate : root->findChildren<QObject *>()) {
        const QVariant interval = candidate->property("interval");
        if (interval.isValid() && interval.toInt() == 1600) {
            restartTimer = candidate;
            break;
        }
    }
    QVERIFY(restartTimer != nullptr);

    controller.play();
    fakeAudio->finishCurrentBuffer();
    QCoreApplication::processEvents();
    QVERIFY(restartBadge->property("visible").toBool());
    QVERIFY(restartTimer->property("running").toBool());

    controller.setLoopEnabled(false);
    QCoreApplication::processEvents();
    QVERIFY(controller.playing());
    QVERIFY(!restartTimer->property("running").toBool());
    QVERIFY(!restartBadge->property("visible").toBool());

    controller.setLoopEnabled(true);
    QCoreApplication::processEvents();
    QVERIFY(controller.playing());
    QVERIFY(!restartBadge->property("visible").toBool());

    QTest::qWait(1700);
    QVERIFY(!restartBadge->property("visible").toBool());
}

void QmlSmokeTest::clearsLoopRestartNoticeForStopAndReplay()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<QmlAudioEngine>();
    QmlAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true, std::move(audio));
    controller.setLoopEnabled(true);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);
    QObject *root = engine.rootObjects().constFirst();
    QObject *restartBadge = root->findChild<QObject *>("loopRestartBadge");
    QVERIFY(restartBadge != nullptr);

    controller.play();
    fakeAudio->finishCurrentBuffer();
    QCoreApplication::processEvents();
    QVERIFY(restartBadge->property("visible").toBool());

    controller.stop();
    controller.play();
    QCoreApplication::processEvents();

    QVERIFY(!restartBadge->property("visible").toBool());
}

void QmlSmokeTest::clearsLoopRestartNoticeForSoundPreview()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<QmlAudioEngine>();
    QmlAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true, std::move(audio));
    controller.setLoopEnabled(true);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);
    QObject *root = engine.rootObjects().constFirst();
    QObject *restartBadge = root->findChild<QObject *>("loopRestartBadge");
    QVERIFY(restartBadge != nullptr);

    controller.play();
    fakeAudio->finishCurrentBuffer();
    QCoreApplication::processEvents();
    QVERIFY(restartBadge->property("visible").toBool());

    QVERIFY(clickQuickItem(root, QStringLiteral("pitchedSoundButton-1")));

    QVERIFY(controller.playing());
    QCOMPARE(controller.playbackStep(), -1);
    QVERIFY(!restartBadge->property("visible").toBool());
}

void QmlSmokeTest::clearsPlaybackIndicatorsForUndoMutation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<QmlAudioEngine>();
    QmlAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true, std::move(audio));
    controller.selectPitched(1);
    QVERIFY(controller.placePitched(0, 3));
    QVERIFY(controller.placePitched(7, 4));
    controller.setLoopEnabled(true);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);
    QObject *root = engine.rootObjects().constFirst();
    auto *window = qobject_cast<QQuickWindow *>(root);
    QVERIFY(window != nullptr);
    QObject *restartBadge = root->findChild<QObject *>("loopRestartBadge");
    QQuickItem *playhead = findQuickItem(window->contentItem(), "playbackPlayhead");
    QQuickItem *activeToken = findQuickItem(window->contentItem(), "compositionToken-0-3");
    QVERIFY(restartBadge != nullptr);
    QVERIFY(playhead != nullptr);
    QVERIFY(activeToken != nullptr);

    controller.play();
    fakeAudio->finishCurrentBuffer();
    QCoreApplication::processEvents();
    QVERIFY(restartBadge->property("visible").toBool());
    QVERIFY(playhead->property("visible").toBool());
    QVERIFY(activeToken->property("sounding").toBool());

    QVERIFY(controller.undo());
    QCoreApplication::processEvents();

    QVERIFY(!controller.playing());
    QCOMPARE(controller.playbackStep(), -1);
    QCOMPARE(controller.playbackCycle(), 0);
    QVERIFY(!restartBadge->property("visible").toBool());
    QVERIFY(!playhead->property("visible").toBool());
    QVERIFY(!activeToken->property("sounding").toBool());
}

void QmlSmokeTest::playheadMovesWithScrollableTimeline()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<QmlAudioEngine>();
    AppController controller(directory.filePath("autosave.json"), true, std::move(audio));
    while (controller.composition()->measureCount() < 8) {
        QVERIFY(controller.addMeasure());
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    QQuickItem *timeline = findQuickItem(window->contentItem(), "timeline");
    QQuickItem *playhead = findQuickItem(window->contentItem(), "playbackPlayhead");
    QVERIFY(timeline != nullptr);
    QVERIFY(playhead != nullptr);

    controller.play();
    QCoreApplication::processEvents();
    const qreal before = playhead->mapToScene(QPointF()).x();
    QVERIFY(timeline->setProperty("contentX", 120.0));
    QCoreApplication::processEvents();
    const qreal after = playhead->mapToScene(QPointF()).x();
    QVERIFY(qAbs((before - after) - 120.0) < 1.0);
}

void QmlSmokeTest::showsSelectedToolBesidePointerOnlyOverGrid()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    AppController controller(directory.filePath("autosave.json"), false);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);

    QQuickItem *indicator = findQuickItem(window->contentItem(), "activeToolIndicator");
    QQuickItem *indicatorMark = findQuickItem(window->contentItem(), "activeToolIndicatorMark");
    QQuickItem *pitchCell = findQuickItem(window->contentItem(), "pitchCellMouse-0-3");
    QQuickItem *drumCell = findQuickItem(window->contentItem(), "drumCellMouse-0-0");
    QQuickItem *timeline = findQuickItem(window->contentItem(), "timeline");
    QQuickItem *eraserButton = findQuickItem(window->contentItem(), "eraserButton");
    QQuickItem *eraserIcon = findQuickItem(window->contentItem(), "eraserIcon");
    QVERIFY(indicator != nullptr);
    QVERIFY(indicatorMark != nullptr);
    QVERIFY(pitchCell != nullptr);
    QVERIFY(drumCell != nullptr);
    QVERIFY(timeline != nullptr);
    QVERIFY(eraserButton != nullptr);
    QVERIFY(eraserIcon != nullptr);
    QVERIFY(eraserButton->width() >= 44.0 && eraserButton->height() >= 44.0);
    QCOMPARE(pitchCell->property("cursorShape").toInt(), int(Qt::BlankCursor));
    QCOMPARE(drumCell->property("cursorShape").toInt(), int(Qt::BlankCursor));

    QAccessibleInterface *eraserAccessible = QAccessible::queryAccessibleInterface(eraserButton);
    QVERIFY(eraserAccessible != nullptr);
    QCOMPARE(eraserAccessible->text(QAccessible::Name), QStringLiteral("Eraser tool"));

    const QStringList pitchedMarks = {QStringLiteral("▥"), QStringLiteral("◆"),
                                      QStringLiteral("◒"), QStringLiteral("○")};
    const QStringList percussionMarks = {QStringLiteral("●"), QStringLiteral("✦")};
    const QList<QSize> supportedSizes = {QSize(1180, 760), QSize(900, 620)};

    for (const QSize &size : supportedSizes) {
        window->resize(size);
        QVERIFY(timeline->setProperty("contentY", 0.0));
        QCoreApplication::processEvents();
        QCOMPARE(window->size(), size);

        const QPoint gridPoint = pitchCell->mapToScene(
            QPointF(pitchCell->width() / 2.0, pitchCell->height() / 2.0)).toPoint();

        for (int sound = 0; sound < pitchedMarks.size(); ++sound) {
            QVERIFY(clickQuickItem(window,
                                   QStringLiteral("pitchedSoundButton-%1").arg(sound)));
            QTest::mouseMove(window, gridPoint);
            QCoreApplication::processEvents();
            QVERIFY(indicator->isVisible());
            QCOMPARE(indicatorMark->property("text").toString(), pitchedMarks.at(sound));
            QVERIFY(!indicator->mapRectToScene(indicator->boundingRect()).contains(gridPoint));
        }

        for (int sound = 0; sound < percussionMarks.size(); ++sound) {
            QVERIFY(clickQuickItem(window,
                                   QStringLiteral("percussionSoundButton-%1").arg(sound)));
            QTest::mouseMove(window, gridPoint);
            QCoreApplication::processEvents();
            QVERIFY(indicator->isVisible());
            QCOMPARE(indicatorMark->property("text").toString(), percussionMarks.at(sound));
            QVERIFY(!indicator->mapRectToScene(indicator->boundingRect()).contains(gridPoint));
        }

        eraserButton->forceActiveFocus(Qt::TabFocusReason);
        QVERIFY(eraserButton->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Space);
        QCoreApplication::processEvents();
        QVERIFY(window->property("eraseMode").toBool());
        QTest::mouseMove(window, gridPoint);
        QCoreApplication::processEvents();
        QVERIFY(indicator->isVisible());
        QVERIFY(indicator->property("eraserTool").toBool());
        QVERIFY(eraserButton->property("checked").toBool());
        QVERIFY(!indicator->mapRectToScene(indicator->boundingRect()).contains(gridPoint));

        const qreal maximumContentY = qMax(
            0.0, timeline->property("contentHeight").toReal() - timeline->height());
        QVERIFY(timeline->setProperty("contentY", maximumContentY));
        QCoreApplication::processEvents();
        const QPoint drumPoint = drumCell->mapToScene(
            QPointF(drumCell->width() / 2.0, drumCell->height() / 2.0)).toPoint();
        QTest::mouseMove(window, drumPoint);
        QCoreApplication::processEvents();
        QVERIFY(indicator->isVisible());
        QVERIFY(indicator->property("eraserTool").toBool());
        QVERIFY(!indicator->mapRectToScene(indicator->boundingRect()).contains(drumPoint));

        const QPoint controlPoint = eraserButton->mapToScene(
            QPointF(eraserButton->width() / 2.0, eraserButton->height() / 2.0)).toPoint();
        QTest::mouseMove(window, controlPoint);
        QCoreApplication::processEvents();
        QVERIFY(!indicator->isVisible());

        // Restore a sound selection before the next size iteration.
        QVERIFY(clickQuickItem(window, QStringLiteral("pitchedSoundButton-0")));
    }
}

QTEST_MAIN(QmlSmokeTest)
#include "tst_qmlsmoke.moc"
