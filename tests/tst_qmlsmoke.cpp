#include <QtTest>
#include <QtTest/qtestaccessible.h>
#include <QAccessible>
#include <QDir>
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

    [[nodiscard]] bool resourcesReleased() const noexcept
    {
        return !activeBufferOpen() && bufferedByteCount() == 0;
    }

    int startCalls = 0;

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
        ++startCalls;
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
    void removesMeasuresWithAccessiblePointerAndKeyboardControl();
    void clearSongDialogCancelsWithoutSideEffects();
    void clearSongDialogConfirmsWithAccessibleControls();
    void controlsPlayStopAndLoopThroughQml();
    void previewThenPlayUsesCompositionStateThroughQml();
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
    void removesMeasureDuringPlaybackAndUndoRestoresIt();
    void playheadMovesWithScrollableTimeline();
    void keepsScrolledTimelineInRangeAfterMeasureRemoval();
    void showsSelectedToolBesidePointerOnlyOverGrid();
    void keepsToolIndicatorOutsideDestinationCells();
    void restoresPointerAfterWindowDeactivationAndReentry();
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

void QmlSmokeTest::removesMeasuresWithAccessiblePointerAndKeyboardControl()
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
    QQuickItem *button = findQuickItem(window->contentItem(),
                                       QStringLiteral("removeMeasureButton"));
    QVERIFY(button != nullptr);
    QVERIFY(!button->property("enabled").toBool());
    QVERIFY(button->property("activeFocusOnTab").toBool());

    QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(button);
    QVERIFY(accessible != nullptr);
    QCOMPARE(accessible->text(QAccessible::Name), QStringLiteral("Remove final measure"));
    QCOMPARE(accessible->role(), QAccessible::Button);

    const QString screenshotDirectory = qEnvironmentVariable("CAIRN_SCREENSHOT_DIR");
    const QList<QSize> supportedSizes = {QSize(1180, 760), QSize(900, 620)};
    for (const QSize &size : supportedSizes) {
        window->resize(size);
        QCoreApplication::processEvents();
        const QRectF buttonRect = button->mapRectToScene(button->boundingRect());
        QVERIFY(QRectF(QPointF(0, 0), size).contains(buttonRect));
        QVERIFY(buttonRect.width() >= 44.0);
        QVERIFY(buttonRect.height() >= 44.0);

        if (!screenshotDirectory.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshotDirectory));
            const QString path = QStringLiteral("%1/remove-measure-%2x%3.png")
                                     .arg(screenshotDirectory)
                                     .arg(size.width())
                                     .arg(size.height());
            QVERIFY2(window->grabWindow().save(path), qPrintable(path));
        }
    }

    QVERIFY(clickQuickItem(window, QStringLiteral("removeMeasureButton")));
    QCOMPARE(controller.composition()->measureCount(), 2);
    QVERIFY(clickQuickItem(window, QStringLiteral("addMeasureButton")));
    QCOMPARE(controller.composition()->measureCount(), 3);
    QVERIFY(button->property("enabled").toBool());

    controller.selectPitched(3);
    QVERIFY(controller.placePitched(8, 6));
    controller.selectPercussion(1);
    QVERIFY(controller.placePercussion(11));
    const QJsonObject beforeRemoval = controller.composition()->toJson();
    QVERIFY(clickQuickItem(window, QStringLiteral("removeMeasureButton")));
    QCOMPARE(controller.composition()->measureCount(), 2);
    QCOMPARE(controller.composition()->rowCount(), 0);
    QVERIFY(clickQuickItem(window, QStringLiteral("undoButton")));
    QCOMPARE(controller.composition()->toJson(), beforeRemoval);
    QVERIFY(clickQuickItem(window, QStringLiteral("removeMeasureButton")));
    QCOMPARE(controller.composition()->measureCount(), 2);

    QVERIFY(clickQuickItem(window, QStringLiteral("addMeasureButton")));
    button->forceActiveFocus(Qt::TabFocusReason);
    QVERIFY(button->hasActiveFocus());
    QTest::keyClick(window, Qt::Key_Space);
    QCoreApplication::processEvents();
    QCOMPARE(controller.composition()->measureCount(), 2);

    while (controller.composition()->measureCount() < 8) {
        QVERIFY(clickQuickItem(window, QStringLiteral("addMeasureButton")));
    }
    for (int expected = 7; expected >= 2; --expected) {
        QVERIFY(clickQuickItem(window, QStringLiteral("removeMeasureButton")));
        QCOMPARE(controller.composition()->measureCount(), expected);
    }
    QVERIFY(!button->property("enabled").toBool());
}

void QmlSmokeTest::clearSongDialogCancelsWithoutSideEffects()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<QmlAudioEngine>();
    AppController controller(directory.filePath("autosave.json"), true, std::move(audio));
    QVERIFY(controller.addMeasure());
    controller.selectPitched(2);
    QVERIFY(controller.placePitched(8, 4));
    const QJsonObject beforeClear = controller.composition()->toJson();
    controller.setLoopEnabled(true);
    controller.play();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    QObject *dialog = window->findChild<QObject *>("clearSongDialog");
    QVERIFY(dialog != nullptr);

    QVERIFY(clickQuickItem(window, QStringLiteral("clearSongButton")));
    QVERIFY(dialog->property("visible").toBool());
    QCOMPARE(controller.composition()->toJson(), beforeClear);
    QVERIFY(controller.playing());

    QVERIFY(clickQuickItem(window, QStringLiteral("cancelClearSongButton")));
    QVERIFY(!dialog->property("visible").toBool());
    QCOMPARE(controller.composition()->toJson(), beforeClear);
    QVERIFY(controller.playing());

    QQuickItem *clearButton = findQuickItem(window->contentItem(),
                                            QStringLiteral("clearSongButton"));
    QVERIFY(clearButton != nullptr);
    clearButton->forceActiveFocus(Qt::TabFocusReason);
    QTest::keyClick(window, Qt::Key_Space);
    QCoreApplication::processEvents();
    QVERIFY(dialog->property("visible").toBool());
    QTest::keyClick(window, Qt::Key_Escape);
    QCoreApplication::processEvents();
    QVERIFY(!dialog->property("visible").toBool());
    QCOMPARE(controller.composition()->toJson(), beforeClear);
    QVERIFY(controller.playing());
}

void QmlSmokeTest::clearSongDialogConfirmsWithAccessibleControls()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<QmlAudioEngine>();
    QmlAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true, std::move(audio));
    QVERIFY(controller.addMeasure());
    controller.selectPitched(3);
    QVERIFY(controller.placePitched(8, 6));
    const QJsonObject beforeClear = controller.composition()->toJson();
    controller.setLoopEnabled(true);
    controller.play();
    fakeAudio->finishCurrentBuffer();
    QCOMPARE(controller.playbackCycle(), 1);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    QQuickItem *clearButton = findQuickItem(window->contentItem(),
                                            QStringLiteral("clearSongButton"));
    QVERIFY(clearButton != nullptr);
    QCOMPARE(clearButton->property("text").toString(), QStringLiteral("Clear Song"));
    QVERIFY(clearButton->property("activeFocusOnTab").toBool());
    QAccessibleInterface *clearAccessible = QAccessible::queryAccessibleInterface(clearButton);
    QVERIFY(clearAccessible != nullptr);
    QCOMPARE(clearAccessible->text(QAccessible::Name), QStringLiteral("Clear Song"));
    QCOMPARE(clearAccessible->role(), QAccessible::Button);

    const QString screenshotDirectory = qEnvironmentVariable("CAIRN_SCREENSHOT_DIR");
    const QList<QSize> supportedSizes = {QSize(1180, 760), QSize(900, 620)};
    for (const QSize &size : supportedSizes) {
        window->resize(size);
        QCoreApplication::processEvents();
        const QRectF buttonRect = clearButton->mapRectToScene(clearButton->boundingRect());
        QVERIFY(QRectF(QPointF(0, 0), size).contains(buttonRect));
        QVERIFY(buttonRect.width() >= 44.0);
        QVERIFY(buttonRect.height() >= 44.0);

        QVERIFY(clickQuickItem(window, QStringLiteral("clearSongButton")));
        QObject *dialog = window->findChild<QObject *>("clearSongDialog");
        QVERIFY(dialog != nullptr);
        QVERIFY(dialog->property("visible").toBool());
        QObject *message = window->findChild<QObject *>("clearSongMessage");
        QVERIFY(message != nullptr);
        QCOMPARE(message->property("text").toString(),
                 QStringLiteral("The current song will be cleared."));
        QQuickItem *cancelButton = findQuickItem(window->contentItem(),
                                                 QStringLiteral("cancelClearSongButton"));
        QQuickItem *confirmButton = findQuickItem(window->contentItem(),
                                                  QStringLiteral("confirmClearSongButton"));
        QVERIFY(cancelButton != nullptr);
        QVERIFY(confirmButton != nullptr);
        QVERIFY(cancelButton->width() >= 44.0 && cancelButton->height() >= 44.0);
        QVERIFY(confirmButton->width() >= 44.0 && confirmButton->height() >= 44.0);
        QAccessibleInterface *cancelAccessible = QAccessible::queryAccessibleInterface(cancelButton);
        QAccessibleInterface *confirmAccessible = QAccessible::queryAccessibleInterface(confirmButton);
        QVERIFY(cancelAccessible != nullptr);
        QVERIFY(confirmAccessible != nullptr);
        QCOMPARE(cancelAccessible->text(QAccessible::Name),
                 QStringLiteral("Cancel clearing song"));
        QCOMPARE(confirmAccessible->text(QAccessible::Name),
                 QStringLiteral("Confirm Clear Song"));
        QVERIFY(!window->findChild<QObject *>("fileDialog"));

        if (!screenshotDirectory.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshotDirectory));
            const QString path = QStringLiteral("%1/clear-song-confirm-%2x%3.png")
                                     .arg(screenshotDirectory)
                                     .arg(size.width())
                                     .arg(size.height());
            QVERIFY2(window->grabWindow().save(path), qPrintable(path));
        }

        if (size != supportedSizes.constLast()) {
            QVERIFY(clickQuickItem(window, QStringLiteral("cancelClearSongButton")));
        }
    }

    QVERIFY(clickQuickItem(window, QStringLiteral("confirmClearSongButton")));
    QCOMPARE(controller.composition()->measureCount(), 2);
    QCOMPARE(controller.composition()->rowCount(), 0);
    QVERIFY(!controller.playing());
    QVERIFY(fakeAudio->resourcesReleased());
    QCOMPARE(controller.playbackStep(), -1);
    QCOMPARE(controller.playbackCycle(), 0);
    QVERIFY(controller.loopEnabled());

    QVERIFY(clickQuickItem(window, QStringLiteral("undoButton")));
    QCOMPARE(controller.composition()->toJson(), beforeClear);
    QVERIFY(findQuickItem(window->contentItem(), QStringLiteral("addMeasureButton")) != nullptr);
    QVERIFY(findQuickItem(window->contentItem(), QStringLiteral("removeMeasureButton")) != nullptr);
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

void QmlSmokeTest::previewThenPlayUsesCompositionStateThroughQml()
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
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    QQuickItem *playButton = findQuickItem(window->contentItem(), "playButton");
    QQuickItem *playhead = findQuickItem(window->contentItem(), "playbackPlayhead");
    QObject *status = window->findChild<QObject *>("playbackStatus");
    QObject *restartBadge = window->findChild<QObject *>("loopRestartBadge");
    QVERIFY(playButton != nullptr);
    QVERIFY(playhead != nullptr);
    QVERIFY(status != nullptr);
    QVERIFY(restartBadge != nullptr);

    QVERIFY(clickQuickItem(window, QStringLiteral("loopCheckBox")));
    QVERIFY(controller.loopEnabled());
    QVERIFY(clickQuickItem(window, QStringLiteral("pitchCellMouse-0-3")));
    QVERIFY(controller.playing());
    QVERIFY(!controller.compositionPlaying());
    QCOMPARE(fakeAudio->startCalls, 1);
    QVERIFY(playButton->property("text").toString().contains(QStringLiteral("Play")));
    QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(playButton);
    QVERIFY(accessible != nullptr);
    QCOMPARE(accessible->text(QAccessible::Name), QStringLiteral("Play song"));
    QVERIFY(!playhead->property("visible").toBool());
    QVERIFY(!status->property("visible").toBool());
    QVERIFY(!restartBadge->property("visible").toBool());

    const QString screenshotDirectory = qEnvironmentVariable("CAIRN_SCREENSHOT_DIR");
    const QList<QSize> supportedSizes = {QSize(1180, 760), QSize(900, 620)};
    for (const QSize &size : supportedSizes) {
        window->resize(size);
        QCoreApplication::processEvents();
        QCOMPARE(window->size(), size);
        QVERIFY(playButton->property("text").toString().contains(QStringLiteral("Play")));
        QCOMPARE(accessible->text(QAccessible::Name), QStringLiteral("Play song"));
        if (!screenshotDirectory.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshotDirectory));
            const QString path = QStringLiteral("%1/preview-play-control-%2x%3.png")
                                     .arg(screenshotDirectory)
                                     .arg(size.width())
                                     .arg(size.height());
            QVERIFY2(window->grabWindow().save(path), qPrintable(path));
        }
    }

    window->resize(supportedSizes.constFirst());
    QCoreApplication::processEvents();
    QCOMPARE(window->size(), supportedSizes.constFirst());

    const QRectF windowRect(QPointF(0, 0), window->size());
    const QRect clickWindowRect(QPoint(0, 0), window->size());
    const QRectF playButtonRect = playButton->mapRectToScene(playButton->boundingRect());
    const QPoint playClickPoint = playButton->mapToScene(
        QPointF(playButton->width() / 2.0, playButton->height() / 2.0)).toPoint();
    QVERIFY2(windowRect.contains(playButtonRect),
             qPrintable(QStringLiteral("Play bounds %1,%2 %3x%4 are outside window %5x%6")
                            .arg(playButtonRect.x()).arg(playButtonRect.y())
                            .arg(playButtonRect.width()).arg(playButtonRect.height())
                            .arg(window->width()).arg(window->height())));
    QVERIFY2(clickWindowRect.contains(playClickPoint),
             qPrintable(QStringLiteral("Play click point %1,%2 is outside window %3x%4")
                            .arg(playClickPoint.x()).arg(playClickPoint.y())
                            .arg(window->width()).arg(window->height())));

    QVERIFY(clickQuickItem(window, QStringLiteral("playButton")));
    QVERIFY(controller.compositionPlaying());
    QCOMPARE(fakeAudio->startCalls, 2);
    QVERIFY(fakeAudio->loopEnabled());
    QVERIFY(playButton->property("text").toString().contains(QStringLiteral("Stop")));
    QCOMPARE(accessible->text(QAccessible::Name), QStringLiteral("Stop song"));
    QVERIFY(playhead->property("visible").toBool());
    QVERIFY(status->property("visible").toBool());

    fakeAudio->finishCurrentBuffer();
    QCoreApplication::processEvents();
    QVERIFY(controller.compositionPlaying());
    QCOMPARE(controller.playbackCycle(), 1);
    QVERIFY(restartBadge->property("visible").toBool());

    QVERIFY(clickQuickItem(window, QStringLiteral("playButton")));
    QVERIFY(!controller.compositionPlaying());
    QVERIFY(!controller.playing());
    QVERIFY(playButton->property("text").toString().contains(QStringLiteral("Play")));
    QCOMPARE(accessible->text(QAccessible::Name), QStringLiteral("Play song"));
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

void QmlSmokeTest::removesMeasureDuringPlaybackAndUndoRestoresIt()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<QmlAudioEngine>();
    QmlAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true, std::move(audio));
    QVERIFY(controller.addMeasure());
    controller.selectPitched(2);
    QVERIFY(controller.placePitched(8, 5));
    controller.selectPercussion(1);
    QVERIFY(controller.placePercussion(11));
    controller.stop();
    const QJsonObject beforeRemoval = controller.composition()->toJson();
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
    QVERIFY(restartBadge != nullptr);
    QVERIFY(playhead != nullptr);

    QVERIFY(clickQuickItem(window, QStringLiteral("playButton")));
    fakeAudio->finishCurrentBuffer();
    QCoreApplication::processEvents();
    QVERIFY(controller.playing());
    QCOMPARE(controller.playbackCycle(), 1);
    QVERIFY(restartBadge->property("visible").toBool());

    QVERIFY(clickQuickItem(window, QStringLiteral("removeMeasureButton")));
    QCOMPARE(controller.composition()->measureCount(), 2);
    QCOMPARE(controller.composition()->rowCount(), 0);
    QVERIFY(!controller.playing());
    QVERIFY(fakeAudio->resourcesReleased());
    QCOMPARE(controller.playbackStep(), -1);
    QCOMPARE(controller.playbackCycle(), 0);
    QVERIFY(!restartBadge->property("visible").toBool());
    QVERIFY(!playhead->property("visible").toBool());

    QVERIFY(clickQuickItem(window, QStringLiteral("undoButton")));
    QCOMPARE(controller.composition()->toJson(), beforeRemoval);
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

void QmlSmokeTest::keepsScrolledTimelineInRangeAfterMeasureRemoval()
{
    const QList<QSize> supportedSizes = {QSize(1180, 760), QSize(900, 620)};
    for (const QSize &size : supportedSizes) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppController controller(directory.filePath("autosave.json"), false);
        while (controller.composition()->measureCount() < 8) {
            QVERIFY(controller.addMeasure());
        }

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("app", &controller);
        engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window != nullptr);
        window->resize(size);
        QCoreApplication::processEvents();

        QQuickItem *timeline = findQuickItem(window->contentItem(), "timeline");
        QVERIFY(timeline != nullptr);
        const qreal oldMaximum = qMax(
            0.0, timeline->property("contentWidth").toReal() - timeline->width());
        QVERIFY(oldMaximum > 0.0);
        QVERIFY(timeline->setProperty("contentX", oldMaximum));
        QCoreApplication::processEvents();

        for (int expected = 7; expected >= 2; --expected) {
            QVERIFY(controller.removeMeasure());
            QCoreApplication::processEvents();
            QCOMPARE(controller.composition()->measureCount(), expected);
            const qreal newMaximum = qMax(
                0.0, timeline->property("contentWidth").toReal() - timeline->width());
            QVERIFY2(timeline->property("contentX").toReal() <= newMaximum + 0.5,
                     qPrintable(QStringLiteral(
                         "contentX %1 exceeds maximum %2 at %3 measures and %4x%5")
                                    .arg(timeline->property("contentX").toReal())
                                    .arg(newMaximum).arg(expected)
                                    .arg(size.width()).arg(size.height())));
            QVERIFY(timeline->property("contentX").toReal() >= 0.0);
        }
    }
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
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));

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

void QmlSmokeTest::keepsToolIndicatorOutsideDestinationCells()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    AppController controller(directory.filePath("autosave.json"), false);
    while (controller.composition()->measureCount() < 8) {
        QVERIFY(controller.addMeasure());
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);

    QQuickItem *indicator = findQuickItem(window->contentItem(), "activeToolIndicator");
    QQuickItem *timeline = findQuickItem(window->contentItem(), "timeline");
    QQuickItem *firstPitch = findQuickItem(window->contentItem(), "pitchCellMouse-0-3");
    QQuickItem *lastPitch = findQuickItem(window->contentItem(), "pitchCellMouse-31-3");
    QQuickItem *lastDrum = findQuickItem(window->contentItem(), "drumCellMouse-31-0");
    QVERIFY(indicator != nullptr);
    QVERIFY(timeline != nullptr);
    QVERIFY(firstPitch != nullptr);
    QVERIFY(lastPitch != nullptr);
    QVERIFY(lastDrum != nullptr);

    const auto verifyPlacement = [window, indicator, timeline](QQuickItem *destination) {
        const QPoint pointer = destination->mapToScene(
            QPointF(destination->width() / 2.0, destination->height() / 2.0)).toPoint();
        QTest::mouseMove(window, pointer);
        QCoreApplication::processEvents();
        QVERIFY(indicator->isVisible());

        const QRectF indicatorRect = indicator->mapRectToScene(indicator->boundingRect());
        const QRectF destinationRect = destination->mapRectToScene(destination->boundingRect());
        const QRectF viewportRect = timeline->mapRectToScene(timeline->boundingRect());
        QVERIFY(!indicatorRect.intersects(destinationRect));
        QVERIFY(viewportRect.contains(indicatorRect));
    };

    const QList<QSize> supportedSizes = {QSize(1180, 760), QSize(900, 620)};
    const QString screenshotDirectory = qEnvironmentVariable("CAIRN_SCREENSHOT_DIR");
    for (const QSize &size : supportedSizes) {
        window->resize(size);
        QVERIFY(timeline->setProperty("contentX", 0.0));
        QVERIFY(timeline->setProperty("contentY", 0.0));
        QCoreApplication::processEvents();
        QCOMPARE(window->size(), size);
        verifyPlacement(firstPitch);

        const qreal maximumContentX = qMax(
            0.0, timeline->property("contentWidth").toReal() - timeline->width());
        QVERIFY(timeline->setProperty("contentX", maximumContentX));
        QCoreApplication::processEvents();
        verifyPlacement(lastPitch);

        const qreal maximumContentY = qMax(
            0.0, timeline->property("contentHeight").toReal() - timeline->height());
        QVERIFY(timeline->setProperty("contentY", maximumContentY));
        QCoreApplication::processEvents();
        verifyPlacement(lastDrum);

        if (!screenshotDirectory.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshotDirectory));
            const QString path = QStringLiteral("%1/pointer-%2x%3.png")
                                     .arg(screenshotDirectory)
                                     .arg(size.width())
                                     .arg(size.height());
            QVERIFY2(window->grabWindow().save(path), qPrintable(path));
        }
    }
}

void QmlSmokeTest::restoresPointerAfterWindowDeactivationAndReentry()
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
    QQuickItem *pitchCell = findQuickItem(window->contentItem(), "pitchCellMouse-0-3");
    QQuickItem *eraserButton = findQuickItem(window->contentItem(), "eraserButton");
    QVERIFY(indicator != nullptr);
    QVERIFY(pitchCell != nullptr);
    QVERIFY(eraserButton != nullptr);

    const QPoint gridPoint = pitchCell->mapToScene(
        QPointF(pitchCell->width() / 2.0, pitchCell->height() / 2.0)).toPoint();
    const QPoint controlPoint = eraserButton->mapToScene(
        QPointF(eraserButton->width() / 2.0, eraserButton->height() / 2.0)).toPoint();
    QTest::mouseMove(window, controlPoint);
    QTest::mouseMove(window, gridPoint);
    QCoreApplication::processEvents();
    QVERIFY(indicator->isVisible());
    QCOMPARE(window->property("toolPointerActive").toBool(),
             window->isActive()
                 && QGuiApplication::applicationState() == Qt::ApplicationActive);
    QCOMPARE(pitchCell->property("cursorShape").toInt(), int(Qt::BlankCursor));

    QQuickWindow otherWindow;
    otherWindow.setGeometry(0, 0, 80, 80);
    otherWindow.show();
    otherWindow.requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(&otherWindow));
    QVERIFY(!window->isActive());
    QCoreApplication::processEvents();
    QVERIFY(!indicator->isVisible());
    QCOMPARE(window->property("toolPointerActive").toBool(),
             window->isActive()
                 && QGuiApplication::applicationState() == Qt::ApplicationActive);
    QCOMPARE(pitchCell->property("cursorShape").toInt(), int(Qt::ArrowCursor));

    QTest::mouseMove(window, controlPoint);
    QCoreApplication::processEvents();
    QVERIFY(!indicator->isVisible());

    otherWindow.hide();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QTest::mouseMove(window, gridPoint);
    QCoreApplication::processEvents();
    QVERIFY(indicator->isVisible());
    QCOMPARE(pitchCell->property("cursorShape").toInt(), int(Qt::BlankCursor));

    QTest::mouseMove(window, controlPoint);
    QCoreApplication::processEvents();
    QVERIFY(!indicator->isVisible());
    QCOMPARE(window->cursor().shape(), Qt::ArrowCursor);
}

QTEST_MAIN(QmlSmokeTest)
#include "tst_qmlsmoke.moc"
