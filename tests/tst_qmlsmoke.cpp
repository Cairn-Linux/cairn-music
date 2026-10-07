#include <QtTest>
#include <QtTest/qtestaccessible.h>
#include <QAccessible>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTranslator>

#include "AppController.h"

namespace {
class LongMessageTranslator final : public QTranslator
{
public:
    QString translate(const char *, const char *sourceText, const char *, int) const override
    {
        const QString source = QString::fromUtf8(sourceText);
        if (source == QStringLiteral("Your song is here, but it is not saved yet.")) {
            return QStringLiteral("Ihre Komposition ist noch hier, wurde aber nicht gespeichert. Versuchen Sie es erneut.");
        }
        if (source == QStringLiteral("Sound stopped. You can try Play again.")) {
            return QStringLiteral("Die Audiowiedergabe wurde angehalten. Ihre Komposition ist sicher. Starten Sie erneut.");
        }
        if (source == QStringLiteral("Only three sounds can play here.")) {
            return QStringLiteral("Hier können nur drei Klänge gleichzeitig spielen. Entfernen Sie zuerst einen vorhandenen Klang.");
        }
        return {};
    }
};

class ScopedTranslator final
{
public:
    explicit ScopedTranslator(QTranslator *translator)
        : m_translator(translator)
    {
        QCoreApplication::installTranslator(m_translator);
    }

    ~ScopedTranslator() { QCoreApplication::removeTranslator(m_translator); }

private:
    QTranslator *m_translator;
};

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
    if (!item && window) {
        item = window->findChild<QQuickItem *>(objectName);
    }
    if (!window || !item || !item->isVisible()) {
        return false;
    }

    const QRectF sceneBounds = item->mapRectToScene(item->boundingRect());
    const QPointF scenePoint = item->mapToScene(item->boundingRect().center());
    const QRectF visibleWindow(QPointF(0, 0), QSizeF(window->width(), window->height()));
    if (sceneBounds.isEmpty() || !visibleWindow.contains(sceneBounds)
        || !sceneBounds.contains(scenePoint) || !visibleWindow.contains(scenePoint)) {
        return false;
    }

    for (QQuickItem *ancestor = item->parentItem(); ancestor;
         ancestor = ancestor->parentItem()) {
        if (ancestor->objectName() != QStringLiteral("timeline")) {
            continue;
        }
        const QRectF viewport = ancestor->mapRectToScene(ancestor->boundingRect());
        if (!viewport.contains(sceneBounds) || !viewport.contains(scenePoint)) {
            return false;
        }
        break;
    }

    QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, scenePoint.toPoint());
    QCoreApplication::processEvents();
    return true;
}

qreal relativeLuminance(const QColor &color)
{
    const auto linear = [](qreal channel) {
        return channel <= 0.04045 ? channel / 12.92
                                  : qPow((channel + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * linear(color.redF()) + 0.7152 * linear(color.greenF())
        + 0.0722 * linear(color.blueF());
}

qreal contrastRatio(const QColor &first, const QColor &second)
{
    const qreal bright = qMax(relativeLuminance(first), relativeLuminance(second));
    const qreal dark = qMin(relativeLuminance(first), relativeLuminance(second));
    return (bright + 0.05) / (dark + 0.05);
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
    void reportsRecoveryActionFailureAndClearsItAfterRetry();
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
    void exposesCompositionCellsToKeyboardAndAccessibility();
    void revealsFocusedCompositionCellsInViewport();
    void offersReducedMotionAndTruthfulInactivityGuidance();
    void reduceMotionControlHasContrastInEveryState();
    void focusedPlacedTokensHaveContainedNonColorRings();
    void timelineKeyboardTraversalIsSpatialAndBounded();
    void timelineTabExitSkipsDisabledUndo();
    void qmlRemainsCompatibleWithDeclaredMinimumQt();
    void timelineKeepsFocusAfterKeyboardErase();
    void modalPopupsSuspendAndRestartInactivityGuidance();
    void showsNonColorSelectionCuesAndHonestSoundLabels();
    void keepsSoundLabelsReadableAtSupportedWindowSizes();
    void keepsEveryCompositionLaneVisibleAtSupportedWindowSizes();
    void guidesSelectedDrumToItsVisibleRowThroughPointerPaths();
    void clicksPitchedPlacementPaths();
    void handlesPitchedPlacementFeedback();
    void keepsPlacementFeedbackFromMovingWorkspace();
    void keepsConcurrentNotificationsStableAndReadable();
    void wrapsLocalizedNotificationsWithoutObscuringWorkspace();
    void keepsAddMeasureUsableAtMinimumSize();
    void showsCurrentPlaybackStepAndSoundingEvents();
    void makesLoopRestartVisibleAndAnnouncesIt();
    void clearsLoopRestartNoticeWhenLoopIsToggled();
    void clearsLoopRestartNoticeForStopAndReplay();
    void clearsLoopRestartNoticeForSoundPreview();
    void clearsPlaybackIndicatorsForUndoMutation();
    void removesMeasureDuringPlaybackAndUndoRestoresIt();
    void preservesAnimatingTokenDelegatesAcrossUnrelatedEdits();
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

    QTestAccessibility::initialize();
    QTestAccessibility::clearEvents();
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
    QVERIFY(placed);
    QVERIFY(controller.saveFailed());
    QVERIFY(banner->property("visible").toBool());
    QVERIFY(!controller.pitchedPlacementRejected());
    QVERIFY(!feedback->property("visible").toBool());
    QCOMPARE(controller.composition()->rowCount(), 3);

    QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(banner);
    QVERIFY(accessible != nullptr);
    QCOMPARE(accessible->text(QAccessible::Name), controller.saveFailureMessage());
    QCOMPARE(accessible->role(), QAccessible::AlertMessage);

    bool announced = false;
    for (const QAccessibleEvent *event : QTestAccessibility::events()) {
        if (event->type() != QAccessible::Announcement) {
            continue;
        }
        const auto *announcement = static_cast<const QAccessibleAnnouncementEvent *>(event);
        if (announcement->message() == controller.saveFailureMessage()
            && announcement->politeness() == QAccessible::AnnouncementPoliteness::Polite) {
            announced = true;
            break;
        }
    }
    QVERIFY(announced);

    qDeleteAll(QTestAccessibility::events());
    QTestAccessibility::clearEvents();
    QTestAccessibility::cleanup();
}

void QmlSmokeTest::reportsRecoveryActionFailureAndClearsItAfterRetry()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString autosavePath = directory.filePath("autosave.json");
    const QByteArray malformed = "{not valid json";
    QFile autosave(autosavePath);
    QVERIFY(autosave.open(QIODevice::WriteOnly));
    QCOMPARE(autosave.write(malformed), malformed.size());
    autosave.close();

    bool saveSucceeds = false;
    auto audio = std::make_unique<AudioEngine>(false);
    AppController controller(
        autosavePath, false, std::move(audio),
        [&saveSucceeds](const QString &, const CompositionModel &) { return saveSucceeds; });
    QVERIFY(controller.loadFailed());

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);

    QObject *popup = window->findChild<QObject *>("loadFailurePopup");
    QQuickItem *failure = window->findChild<QQuickItem *>("recoveryActionFailure");
    QQuickItem *recoveryButton = findQuickItem(window->contentItem(), "recoveryActionButton");
    QVERIFY(popup != nullptr);
    QVERIFY(failure != nullptr);
    QVERIFY(recoveryButton != nullptr);
    QVERIFY(recoveryButton->width() >= 44.0 && recoveryButton->height() >= 44.0);
    QVERIFY(popup->property("visible").toBool());
    QVERIFY(!failure->isVisible());

    QTestAccessibility::initialize();
    QTestAccessibility::clearEvents();
    QVERIFY(clickQuickItem(window, QStringLiteral("recoveryActionButton")));
    QVERIFY(controller.loadFailed());
    QVERIFY(popup->property("visible").toBool());
    QVERIFY(failure->isVisible());
    QCOMPARE(failure->property("text").toString(),
             QStringLiteral("We couldn't keep this song safe yet. Please try again."));
    QVERIFY(!failure->property("text").toString().contains(autosavePath));

    QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(failure);
    QVERIFY(accessible != nullptr);
    QCOMPARE(accessible->role(), QAccessible::AlertMessage);
    QCOMPARE(accessible->text(QAccessible::Name), failure->property("text").toString());

    bool announced = false;
    for (const QAccessibleEvent *event : QTestAccessibility::events()) {
        if (event->type() != QAccessible::Announcement) {
            continue;
        }
        const auto *announcement = static_cast<const QAccessibleAnnouncementEvent *>(event);
        if (announcement->message() == failure->property("text").toString()
            && announcement->politeness() == QAccessible::AnnouncementPoliteness::Polite) {
            announced = true;
            break;
        }
    }
    QVERIFY(announced);

    QVERIFY(autosave.open(QIODevice::ReadOnly));
    QCOMPARE(autosave.readAll(), malformed);
    autosave.close();

    const QString screenshotDirectory = qEnvironmentVariable("CAIRN_SCREENSHOT_DIR");
    const QList<QSize> supportedSizes = {QSize(1180, 760), QSize(900, 620)};
    for (const QSize &size : supportedSizes) {
        window->resize(size);
        QCoreApplication::processEvents();
        const QRectF failureRect = failure->mapRectToScene(failure->boundingRect());
        const QRectF windowRect(QPointF(0, 0), size);
        QVERIFY(windowRect.contains(failureRect));
        if (!screenshotDirectory.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshotDirectory));
            const QString path = QStringLiteral("%1/recovery-failure-%2x%3.png")
                                     .arg(screenshotDirectory)
                                     .arg(size.width())
                                     .arg(size.height());
            QVERIFY2(window->grabWindow().save(path), qPrintable(path));
        }
    }

    saveSucceeds = true;
    QVERIFY(clickQuickItem(window, QStringLiteral("recoveryActionButton")));
    QVERIFY(!controller.loadFailed());
    QVERIFY(!popup->property("visible").toBool());
    QVERIFY(!failure->isVisible());

    qDeleteAll(QTestAccessibility::events());
    QTestAccessibility::clearEvents();
    QTestAccessibility::cleanup();
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
        {QStringLiteral("pitchedSoundButton-1"), QStringLiteral("Pluck"),
         QAccessible::CheckBox},
        {QStringLiteral("pitchedSoundButton-2"), QStringLiteral("Bell"),
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

    QQuickItem *eraser = findQuickItem(window->contentItem(), "eraserButton");
    QVERIFY(eraser != nullptr);
    eraser->forceActiveFocus(Qt::TabFocusReason);
    const QStringList expectedTabOrder = {
        QStringLiteral("playButton"),
        QStringLiteral("loopCheckBox"),
        QStringLiteral("pitchedSoundButton-0"),
        QStringLiteral("pitchedSoundButton-1"),
        QStringLiteral("pitchedSoundButton-2"),
        QStringLiteral("pitchedSoundButton-3"),
        QStringLiteral("percussionSoundButton-0"),
        QStringLiteral("percussionSoundButton-1"),
        QStringLiteral("reduceMotionCheckBox"),
        QStringLiteral("clearSongButton"),
        QStringLiteral("addMeasureButton"),
        QStringLiteral("pitchCellMouse-0-6"),
    };
    for (const QString &objectName : expectedTabOrder) {
        QTest::keyClick(window, Qt::Key_Tab);
        QCoreApplication::processEvents();
        QQuickItem *focused = window->activeFocusItem();
        QVERIFY2(focused != nullptr, qPrintable(objectName));
        QCOMPARE(focused->objectName(), objectName);
    }
}

void QmlSmokeTest::exposesCompositionCellsToKeyboardAndAccessibility()
{
    const QList<QSize> supportedSizes = {QSize(1180, 760), QSize(900, 620)};
    for (const QSize &size : supportedSizes) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppController controller(directory.filePath("autosave.json"), false);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("app", &controller);
        engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window != nullptr);
        window->resize(size);
        QCoreApplication::processEvents();

        QQuickItem *timeline = findQuickItem(window->contentItem(), "timeline");
        QQuickItem *pitchCell = findQuickItem(window->contentItem(), "pitchCellMouse-0-3");
        QQuickItem *drumCell = findQuickItem(window->contentItem(), "drumCellMouse-0-0");
        QVERIFY(timeline != nullptr);
        QVERIFY(pitchCell != nullptr);
        QVERIFY(drumCell != nullptr);

        const QRectF viewportRect = timeline->mapRectToScene(timeline->boundingRect());
        const QRectF windowRect(QPointF(0, 0), size);
        for (QQuickItem *cell : {pitchCell, drumCell}) {
            const QRectF cellRect = cell->mapRectToScene(cell->boundingRect());
            const QPointF clickPoint = cell->mapToScene(cell->boundingRect().center());
            QVERIFY2(viewportRect.contains(cellRect), qPrintable(cell->objectName()));
            QVERIFY2(windowRect.contains(cellRect), qPrintable(cell->objectName()));
            QVERIFY2(cellRect.contains(clickPoint), qPrintable(cell->objectName()));
            QVERIFY2(windowRect.contains(clickPoint), qPrintable(cell->objectName()));
            QVERIFY2(cellRect.width() >= 44.0 && cellRect.height() >= 44.0,
                     qPrintable(cell->objectName()));
            QVERIFY2(!cell->property("activeFocusOnTab").toBool(),
                     qPrintable(cell->objectName()));
            QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(cell);
            QVERIFY2(accessible != nullptr, qPrintable(cell->objectName()));
            QCOMPARE(accessible->role(), QAccessible::Button);
            QVERIFY2(!accessible->text(QAccessible::Name).trimmed().isEmpty(),
                     qPrintable(cell->objectName()));
        }

        QAccessibleInterface *pitchAccessible =
            QAccessible::queryAccessibleInterface(pitchCell);
        QVERIFY(pitchAccessible != nullptr);
        QAccessibleActionInterface *pitchAction = pitchAccessible->actionInterface();
        QVERIFY(pitchAction != nullptr);
        QVERIFY(pitchAction->actionNames().contains(QAccessibleActionInterface::pressAction()));
        pitchAction->doAction(QAccessibleActionInterface::pressAction());
        QCoreApplication::processEvents();
        QCOMPARE(controller.composition()->rowCount(), 1);

        pitchCell->forceActiveFocus(Qt::TabFocusReason);
        QVERIFY(pitchCell->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Space);
        QCoreApplication::processEvents();
        QCOMPARE(controller.composition()->rowCount(), 1);
        const QModelIndex placed = controller.composition()->index(0);
        QCOMPARE(controller.composition()->data(placed, CompositionModel::StepRole).toInt(), 0);
        QCOMPARE(controller.composition()->data(placed, CompositionModel::PitchRowRole).toInt(), 3);
        QQuickItem *pitchedToken = findQuickItem(window->contentItem(), "pitchedTokenMouse-0-3");
        QVERIFY(pitchedToken != nullptr);
        const QRectF pitchedTokenRect = pitchedToken->mapRectToScene(pitchedToken->boundingRect());
        QVERIFY(viewportRect.contains(pitchedTokenRect));
        QVERIFY(windowRect.contains(pitchedTokenRect));
        QVERIFY(pitchedTokenRect.width() >= 44.0 && pitchedTokenRect.height() >= 44.0);
        QAccessibleInterface *pitchedTokenAccessible =
            QAccessible::queryAccessibleInterface(pitchedToken);
        QVERIFY(pitchedTokenAccessible != nullptr);
        QCOMPARE(pitchedTokenAccessible->role(), QAccessible::Button);
        QVERIFY(!pitchedTokenAccessible->text(QAccessible::Name).trimmed().isEmpty());
        QAccessibleActionInterface *pitchedTokenAction =
            pitchedTokenAccessible->actionInterface();
        QVERIFY(pitchedTokenAction != nullptr);
        QVERIFY(pitchedTokenAction->actionNames().contains(
            QAccessibleActionInterface::pressAction()));
        pitchedTokenAction->doAction(QAccessibleActionInterface::pressAction());
        QCoreApplication::processEvents();
        QCOMPARE(controller.composition()->rowCount(), 1);

        QVERIFY(clickQuickItem(window, QStringLiteral("percussionSoundButton-0")));
        QAccessibleInterface *drumAccessible =
            QAccessible::queryAccessibleInterface(drumCell);
        QVERIFY(drumAccessible != nullptr);
        QAccessibleActionInterface *drumAction = drumAccessible->actionInterface();
        QVERIFY(drumAction != nullptr);
        QVERIFY(drumAction->actionNames().contains(QAccessibleActionInterface::pressAction()));
        drumAction->doAction(QAccessibleActionInterface::pressAction());
        QCoreApplication::processEvents();
        QCOMPARE(controller.composition()->rowCount(), 2);

        drumCell->forceActiveFocus(Qt::TabFocusReason);
        QVERIFY(drumCell->hasActiveFocus());
        QTest::keyClick(window, Qt::Key_Return);
        QCoreApplication::processEvents();
        QCOMPARE(controller.composition()->rowCount(), 2);
        QQuickItem *percussionToken = findQuickItem(
            window->contentItem(), "percussionTokenMouse-0-0");
        QVERIFY(percussionToken != nullptr);
        const QRectF percussionTokenRect =
            percussionToken->mapRectToScene(percussionToken->boundingRect());
        QVERIFY(viewportRect.contains(percussionTokenRect));
        QVERIFY(windowRect.contains(percussionTokenRect));
        QVERIFY(percussionTokenRect.width() >= 44.0 && percussionTokenRect.height() >= 44.0);
        QAccessibleInterface *percussionTokenAccessible =
            QAccessible::queryAccessibleInterface(percussionToken);
        QVERIFY(percussionTokenAccessible != nullptr);
        QCOMPARE(percussionTokenAccessible->role(), QAccessible::Button);
        QVERIFY(!percussionTokenAccessible->text(QAccessible::Name).trimmed().isEmpty());
        QAccessibleActionInterface *percussionTokenAction =
            percussionTokenAccessible->actionInterface();
        QVERIFY(percussionTokenAction != nullptr);
        QVERIFY(percussionTokenAction->actionNames().contains(
            QAccessibleActionInterface::pressAction()));
        percussionTokenAction->doAction(QAccessibleActionInterface::pressAction());
        QCoreApplication::processEvents();
        QCOMPARE(controller.composition()->rowCount(), 2);
    }
}

void QmlSmokeTest::revealsFocusedCompositionCellsInViewport()
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
    window->resize(900, 620);
    QCoreApplication::processEvents();

    QQuickItem *timeline = findQuickItem(window->contentItem(), "timeline");
    QQuickItem *lastPitch = findQuickItem(window->contentItem(), "pitchCellMouse-31-6");
    QQuickItem *lastDrum = findQuickItem(window->contentItem(), "drumCellMouse-31-1");
    QVERIFY(timeline != nullptr);
    QVERIFY(lastPitch != nullptr);
    QVERIFY(lastDrum != nullptr);

    const auto viewportRect = [timeline]() {
        return timeline->mapRectToScene(timeline->boundingRect());
    };
    const auto itemRect = [](QQuickItem *item) {
        return item->mapRectToScene(item->boundingRect());
    };

    QVERIFY(!viewportRect().contains(itemRect(lastPitch)));
    lastPitch->forceActiveFocus(Qt::TabFocusReason);
    QCoreApplication::processEvents();
    QVERIFY(lastPitch->hasActiveFocus());
    QVERIFY(viewportRect().contains(itemRect(lastPitch)));
    QVERIFY(timeline->property("contentX").toReal() > 0.0);

    QVERIFY(timeline->setProperty("contentX", 0.0));
    QCoreApplication::processEvents();
    QVERIFY(!viewportRect().contains(itemRect(lastDrum)));
    lastDrum->forceActiveFocus(Qt::TabFocusReason);
    QCoreApplication::processEvents();
    QVERIFY(lastDrum->hasActiveFocus());
    QVERIFY(viewportRect().contains(itemRect(lastDrum)));
    QVERIFY(timeline->property("contentX").toReal() > 0.0);
}

void QmlSmokeTest::offersReducedMotionAndTruthfulInactivityGuidance()
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

    QQuickItem *reduceMotion = findQuickItem(window->contentItem(), "reduceMotionCheckBox");
    QQuickItem *hint = findQuickItem(window->contentItem(), "inactivityHint");
    QObject *hintAnimation = hint ? hint->findChild<QObject *>("inactivityHintAnimation")
                                  : nullptr;
    QVERIFY(reduceMotion != nullptr);
    QVERIFY(hint != nullptr);
    QVERIFY(hintAnimation != nullptr);
    QVERIFY(reduceMotion->property("activeFocusOnTab").toBool());
    QVERIFY(reduceMotion->width() >= 44.0 && reduceMotion->height() >= 44.0);
    QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(reduceMotion);
    QVERIFY(accessible != nullptr);
    QCOMPARE(accessible->role(), QAccessible::CheckBox);
    QCOMPARE(accessible->text(QAccessible::Name), QStringLiteral("Reduce motion"));

    QVERIFY(!window->property("reduceMotion").toBool());
    QVERIFY(window->property("motionEnabled").toBool());
    QVERIFY(window->property("placementAnimationsEnabled").toBool());
    QVERIFY(QMetaObject::invokeMethod(window, "handleInactivityTimeout", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QVERIFY(hint->isVisible());
    QVERIFY(hintAnimation->property("running").toBool());

    const QString screenshotDirectory = qEnvironmentVariable("CAIRN_SCREENSHOT_DIR");
    const QList<QSize> supportedSizes = {QSize(1180, 760), QSize(900, 620)};
    for (const QSize &size : supportedSizes) {
        window->resize(size);
        QCoreApplication::processEvents();
        QCOMPARE(window->size(), size);
        const QRectF controlRect = reduceMotion->mapRectToScene(reduceMotion->boundingRect());
        QVERIFY(QRectF(QPointF(0, 0), size).contains(controlRect));
        QVERIFY(controlRect.width() >= 44.0 && controlRect.height() >= 44.0);
        if (!screenshotDirectory.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshotDirectory));
            const QString path = QStringLiteral("%1/inactivity-guidance-%2x%3.png")
                                     .arg(screenshotDirectory)
                                     .arg(size.width()).arg(size.height());
            QVERIFY2(window->grabWindow().save(path), qPrintable(path));
        }
    }

    QVERIFY(clickQuickItem(window, QStringLiteral("reduceMotionCheckBox")));
    QVERIFY(window->property("reduceMotion").toBool());
    QVERIFY(!window->property("motionEnabled").toBool());
    QVERIFY(!window->property("placementAnimationsEnabled").toBool());
    QVERIFY(!hint->isVisible());
    QVERIFY(!hintAnimation->property("running").toBool());

    // Emulate reduced motion being selected before the idle interval; changing
    // the setting itself correctly counts as interaction and dismisses the hint.
    QVERIFY(window->setProperty("hasInteracted", false));
    QVERIFY(QMetaObject::invokeMethod(window, "handleInactivityTimeout", Qt::DirectConnection));
    QCoreApplication::processEvents();
    QVERIFY(hint->isVisible());
    QVERIFY(!hintAnimation->property("running").toBool());

    for (const QSize &size : supportedSizes) {
        window->resize(size);
        QCoreApplication::processEvents();
        QCOMPARE(window->size(), size);
        QVERIFY(hint->isVisible());
        QVERIFY(!hintAnimation->property("running").toBool());
        if (!screenshotDirectory.isEmpty()) {
            const QString path = QStringLiteral("%1/reduced-motion-%2x%3.png")
                                     .arg(screenshotDirectory)
                                     .arg(size.width()).arg(size.height());
            QVERIFY2(window->grabWindow().save(path), qPrintable(path));
        }
    }

    QVERIFY(clickQuickItem(window, QStringLiteral("pitchedSoundButton-0")));
    QVERIFY(!hint->isVisible());

    // Keyboard focus navigation is activity even before a control is activated.
    QVERIFY(window->setProperty("hasInteracted", false));
    QTRY_VERIFY_WITH_TIMEOUT(window->property("focusInteractionArmed").toBool(), 500);
    QQuickItem *playButton = findQuickItem(window->contentItem(), "playButton");
    QVERIFY(playButton != nullptr);
    playButton->forceActiveFocus(Qt::TabFocusReason);
    QCoreApplication::processEvents();
    QVERIFY(window->property("hasInteracted").toBool());
    QVERIFY(QMetaObject::invokeMethod(window, "showInactivityGuidance", Qt::DirectConnection));
    QVERIFY(!hint->isVisible());
}

void QmlSmokeTest::reduceMotionControlHasContrastInEveryState()
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
    QQuickItem *control = findQuickItem(window->contentItem(), "reduceMotionCheckBox");
    QQuickItem *label = findQuickItem(window->contentItem(), "reduceMotionLabel");
    QQuickItem *indicator = findQuickItem(window->contentItem(), "reduceMotionIndicator");
    QQuickItem *mark = findQuickItem(window->contentItem(), "reduceMotionMark");
    QVERIFY(control != nullptr);
    QVERIFY(label != nullptr);
    QVERIFY(indicator != nullptr);
    QVERIFY(mark != nullptr);

    const QColor panel("#36254c");
    const QStringList textTokens = {
        QStringLiteral("normalTextColor"), QStringLiteral("hoverTextColor"),
        QStringLiteral("focusTextColor"), QStringLiteral("checkedTextColor"),
        QStringLiteral("disabledTextColor"),
    };
    for (const QString &property : textTokens) {
        const QColor color(control->property(property.toUtf8().constData()).toString());
        QVERIFY2(color.isValid(), qPrintable(property));
        QVERIFY2(contrastRatio(color, panel) >= 4.5,
                 qPrintable(QStringLiteral("%1 contrast is %2")
                                .arg(property).arg(contrastRatio(color, panel))));
    }
    const QStringList graphicTokens = {
        QStringLiteral("normalIndicatorColor"), QStringLiteral("hoverIndicatorColor"),
        QStringLiteral("focusIndicatorColor"), QStringLiteral("checkedIndicatorColor"),
        QStringLiteral("disabledIndicatorColor"),
    };
    for (const QString &property : graphicTokens) {
        const QColor color(control->property(property.toUtf8().constData()).toString());
        QVERIFY2(color.isValid(), qPrintable(property));
        QVERIFY2(contrastRatio(color, panel) >= 3.0,
                 qPrintable(QStringLiteral("%1 contrast is %2")
                                .arg(property).arg(contrastRatio(color, panel))));
    }

    control->forceActiveFocus(Qt::TabFocusReason);
    QCoreApplication::processEvents();
    QVERIFY(control->hasActiveFocus());
    QCOMPARE(label->property("color").value<QColor>(),
             QColor(control->property("focusTextColor").toString()));
    QVERIFY(indicator->property("border").isValid());
    QVERIFY(clickQuickItem(window, QStringLiteral("reduceMotionCheckBox")));
    QVERIFY(control->property("checked").toBool());
    QVERIFY(mark->isVisible());
    QCOMPARE(mark->property("text").toString(), QStringLiteral("✓"));
    QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(control);
    QVERIFY(accessible != nullptr);
    QVERIFY(accessible->state().checked);

    const QString screenshotDirectory = qEnvironmentVariable("CAIRN_SCREENSHOT_DIR");
    for (const QSize &size : {QSize(1180, 760), QSize(900, 620)}) {
        window->resize(size);
        QCoreApplication::processEvents();
        const QRectF rect = control->mapRectToScene(control->boundingRect());
        QVERIFY(QRectF(QPointF(), size).contains(rect));
        if (!screenshotDirectory.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshotDirectory));
            const QString path = QStringLiteral("%1/reduce-motion-control-%2x%3.png")
                                     .arg(screenshotDirectory).arg(size.width()).arg(size.height());
            QVERIFY2(window->grabWindow().save(path), qPrintable(path));
        }
    }

    control->setEnabled(false);
    QCoreApplication::processEvents();
    QCOMPARE(label->property("color").value<QColor>(),
             QColor(control->property("disabledTextColor").toString()));
    QVERIFY(mark->isVisible());
}

void QmlSmokeTest::focusedPlacedTokensHaveContainedNonColorRings()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    AppController controller(directory.filePath("autosave.json"), false);
    controller.selectPitched(2);
    QVERIFY(controller.placePitched(1, 6));
    controller.selectPercussion(1);
    QVERIFY(controller.placePercussion(2));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    const QString screenshotDirectory = qEnvironmentVariable("CAIRN_SCREENSHOT_DIR");

    for (const QSize &size : {QSize(1180, 760), QSize(900, 620)}) {
        window->resize(size);
        QCoreApplication::processEvents();
        const QList<QPair<QString, QString>> targets = {
            {QStringLiteral("pitchedTokenMouse-1-6"), QStringLiteral("pitchCellMouse-1-6")},
            {QStringLiteral("percussionTokenMouse-2-1"), QStringLiteral("drumCellMouse-2-1")},
        };
        for (const auto &[targetName, cellName] : targets) {
            QQuickItem *target = findQuickItem(window->contentItem(), targetName);
            QQuickItem *cell = findQuickItem(window->contentItem(), cellName);
            const QString ringName = targetName.startsWith(QStringLiteral("pitched"))
                ? QStringLiteral("pitchedTokenFocusRing-1-6")
                : QStringLiteral("percussionTokenFocusRing-2-1");
            QQuickItem *ring = findQuickItem(window->contentItem(), ringName);
            QVERIFY2(target != nullptr, qPrintable(targetName));
            QVERIFY2(cell != nullptr, qPrintable(cellName));
            QVERIFY2(ring != nullptr, qPrintable(ringName));
            target->forceActiveFocus(Qt::TabFocusReason);
            QCoreApplication::processEvents();
            QCOMPARE(window->activeFocusItem(), target);
            QVERIFY(ring->isVisible());
            QVERIFY(ring->property("border").isValid());
            const QRectF cellRect = cell->mapRectToScene(cell->boundingRect());
            const QRectF ringRect = ring->mapRectToScene(ring->boundingRect());
            QVERIFY2(cellRect.contains(ringRect), qPrintable(ringName));
            QVERIFY(ring->property("color").value<QColor>().alpha() == 0);
        }
        QQuickItem *pitched = findQuickItem(window->contentItem(), "pitchedTokenMouse-1-6");
        pitched->forceActiveFocus(Qt::TabFocusReason);
        QCoreApplication::processEvents();
        if (!screenshotDirectory.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshotDirectory));
            const QString path = QStringLiteral("%1/token-focus-%2x%3.png")
                                     .arg(screenshotDirectory).arg(size.width()).arg(size.height());
            QVERIFY2(window->grabWindow().save(path), qPrintable(path));
        }
    }
}

void QmlSmokeTest::timelineKeyboardTraversalIsSpatialAndBounded()
{
    for (const int measures : {2, 8}) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppController controller(directory.filePath("autosave.json"), false);
        while (controller.composition()->measureCount() < measures)
            QVERIFY(controller.addMeasure());
        const int finalStep = measures * 4 - 1;
        controller.selectPercussion(1);
        QVERIFY(controller.placePercussion(finalStep));
        controller.selectPitched(2);
        QVERIFY(controller.placePitched(0, 5));
        QVERIFY(controller.placePitched(finalStep, 6));

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("app", &controller);
        engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window != nullptr);
        window->resize(measures == 8 ? QSize(900, 620) : QSize(1180, 760));
        QCoreApplication::processEvents();
        QQuickItem *timeline = findQuickItem(window->contentItem(), "timeline");
        QQuickItem *add = findQuickItem(window->contentItem(), "addMeasureButton");
        QQuickItem *remove = findQuickItem(window->contentItem(), "removeMeasureButton");
        QVERIFY(timeline != nullptr);
        QVERIFY(add != nullptr);
        QVERIFY(remove != nullptr);
        QVERIFY(timeline->property("activeFocusOnTab").toBool());
        QVERIFY(findQuickItem(window->contentItem(), "pitchCellMouse-0-6")
                    ->property("activeFocusOnTab").toBool());
        QVERIFY(!findQuickItem(window->contentItem(), "pitchCellMouse-1-6")
                     ->property("activeFocusOnTab").toBool());

        QQuickItem *previous = add->isEnabled() ? add : remove;
        previous->forceActiveFocus(Qt::TabFocusReason);
        QTest::keyClick(window, Qt::Key_Tab);
        QCoreApplication::processEvents();
        QCOMPARE(window->activeFocusItem()->objectName(), QStringLiteral("pitchCellMouse-0-6"));
        QTest::keyClick(window, Qt::Key_Down);
        QCOMPARE(window->activeFocusItem()->objectName(), QStringLiteral("pitchedTokenMouse-0-5"));
        for (int lane = 2; lane <= 8; ++lane)
            QTest::keyClick(window, Qt::Key_Down);
        QCOMPARE(window->activeFocusItem()->objectName(), QStringLiteral("drumCellMouse-0-1"));

        for (int step = 1; step <= finalStep; ++step)
            QTest::keyClick(window, Qt::Key_Right);
        QCOMPARE(window->activeFocusItem()->objectName(),
                 QStringLiteral("percussionTokenMouse-%1-1").arg(finalStep));
        const QRectF viewport = timeline->mapRectToScene(timeline->boundingRect());
        QVERIFY(viewport.contains(window->activeFocusItem()->mapRectToScene(
            window->activeFocusItem()->boundingRect())));
        QTest::keyClick(window, Qt::Key_Up);
        for (int lane = 7; lane > 0; --lane)
            QTest::keyClick(window, Qt::Key_Up);
        QCOMPARE(window->activeFocusItem()->objectName(),
                 QStringLiteral("pitchedTokenMouse-%1-6").arg(finalStep));

        QTest::keyClick(window, Qt::Key_Backtab);
        QCOMPARE(window->activeFocusItem(), previous);
        QTest::keyClick(window, Qt::Key_Tab);
        QCOMPARE(window->activeFocusItem()->objectName(), QStringLiteral("pitchCellMouse-0-6"));
        QTest::keyClick(window, Qt::Key_Tab);
        QCOMPARE(window->activeFocusItem()->objectName(), QStringLiteral("undoButton"));
    }
}

void QmlSmokeTest::timelineTabExitSkipsDisabledUndo()
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
    QQuickItem *add = findQuickItem(window->contentItem(), "addMeasureButton");
    QQuickItem *undo = findQuickItem(window->contentItem(), "undoButton");
    QVERIFY(add != nullptr);
    QVERIFY(undo != nullptr);
    QVERIFY(!undo->isEnabled());

    add->forceActiveFocus(Qt::TabFocusReason);
    QTest::keyClick(window, Qt::Key_Tab);
    QCOMPARE(window->activeFocusItem()->objectName(), QStringLiteral("pitchCellMouse-0-6"));
    QTest::keyClick(window, Qt::Key_Tab);
    QCOMPARE(window->activeFocusItem()->objectName(), QStringLiteral("eraserButton"));
}

void QmlSmokeTest::qmlRemainsCompatibleWithDeclaredMinimumQt()
{
    QFile qml(QStringLiteral(CAIRN_MUSIC_QML_PATH));
    QVERIFY(qml.open(QIODevice::ReadOnly));
    const QByteArray source = qml.readAll();
    QVERIFY2(!source.contains("focusPolicy:"),
             "QQuickItem.focusPolicy requires Qt 6.7, but CMake declares Qt 6.2");
}

void QmlSmokeTest::timelineKeepsFocusAfterKeyboardErase()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    AppController controller(directory.filePath("autosave.json"), false);
    QVERIFY(controller.placePitched(0, 6));
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    QQuickItem *token = findQuickItem(window->contentItem(), "pitchedTokenMouse-0-6");
    QVERIFY(token != nullptr);
    QVERIFY(window->setProperty("eraseMode", true));
    token->forceActiveFocus(Qt::TabFocusReason);
    QTest::keyClick(window, Qt::Key_Space);
    QCoreApplication::processEvents();
    QCOMPARE(controller.composition()->rowCount(), 0);
    QCOMPARE(window->activeFocusItem()->objectName(), QStringLiteral("pitchCellMouse-0-6"));
    QTest::keyClick(window, Qt::Key_Right);
    QCOMPARE(window->activeFocusItem()->objectName(), QStringLiteral("pitchCellMouse-1-6"));
}

void QmlSmokeTest::modalPopupsSuspendAndRestartInactivityGuidance()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString autosavePath = directory.filePath("autosave.json");
    QFile autosave(autosavePath);
    QVERIFY(autosave.open(QIODevice::WriteOnly));
    QVERIFY(autosave.write("{broken") > 0);
    autosave.close();
    bool saveSucceeds = false;
    AppController controller(
        autosavePath, false, std::make_unique<AudioEngine>(false),
        [&saveSucceeds](const QString &, const CompositionModel &) { return saveSucceeds; });
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    QObject *popup = window->findChild<QObject *>("loadFailurePopup");
    QObject *timer = window->findChild<QObject *>("inactivityTimer");
    QQuickItem *hint = findQuickItem(window->contentItem(), "inactivityHint");
    QObject *animation = hint ? hint->findChild<QObject *>("inactivityHintAnimation") : nullptr;
    QVERIFY(popup != nullptr);
    QVERIFY(timer != nullptr);
    QVERIFY(hint != nullptr);
    QVERIFY(animation != nullptr);
    QVERIFY(timer->setProperty("interval", 30));
    QCoreApplication::processEvents();
    QVERIFY(popup->property("visible").toBool());
    QVERIFY(!timer->property("running").toBool());
    QVERIFY(!hint->isVisible());
    QVERIFY(!animation->property("running").toBool());
    QTest::qWait(100);
    QVERIFY(!hint->isVisible());

    for (const QSize &size : {QSize(900, 620), QSize(1180, 760)}) {
        window->resize(size);
        QCoreApplication::processEvents();
        QVERIFY(!hint->isVisible());
        QVERIFY(clickQuickItem(window, QStringLiteral("recoveryActionButton")));
        QVERIFY(popup->property("visible").toBool());
        QVERIFY(!timer->property("running").toBool());
    }

    saveSucceeds = true;
    QVERIFY(clickQuickItem(window, QStringLiteral("recoveryActionButton")));
    QVERIFY(!popup->property("visible").toBool());
    QVERIFY(timer->property("running").toBool());
    QTRY_VERIFY_WITH_TIMEOUT(hint->isVisible(), 150);
    QVERIFY(animation->property("running").toBool());

    QVERIFY(QMetaObject::invokeMethod(window, "markInteraction", Qt::DirectConnection));
    QVERIFY(!hint->isVisible());
    QVERIFY(timer->property("running").toBool());
    QTRY_VERIFY_WITH_TIMEOUT(hint->isVisible(), 150);
    QVERIFY(window->setProperty("reduceMotion", true));
    QCoreApplication::processEvents();
    QVERIFY(hint->isVisible());
    QVERIFY(!animation->property("running").toBool());

    QVERIFY(clickQuickItem(window, QStringLiteral("clearSongButton")));
    QObject *clearDialog = window->findChild<QObject *>("clearSongDialog");
    QVERIFY(clearDialog != nullptr);
    QVERIFY(clearDialog->property("visible").toBool());
    QVERIFY(!hint->isVisible());
    QVERIFY(!timer->property("running").toBool());
    QVERIFY(clickQuickItem(window, QStringLiteral("cancelClearSongButton")));
    QVERIFY(timer->property("running").toBool());
    QTRY_VERIFY_WITH_TIMEOUT(hint->isVisible(), 150);
    QVERIFY(!animation->property("running").toBool());
}

void QmlSmokeTest::showsNonColorSelectionCuesAndHonestSoundLabels()
{
    const QList<QSize> supportedSizes = {QSize(1180, 760), QSize(900, 620)};
    for (const QSize &size : supportedSizes) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppController controller(directory.filePath("autosave.json"), false);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("app", &controller);
        engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window != nullptr);
        window->resize(size);
        QCoreApplication::processEvents();

        const QStringList honestNames = {QStringLiteral("Keys"), QStringLiteral("Pluck"),
                                         QStringLiteral("Bell"), QStringLiteral("Bubble")};
        const QRectF windowRect(QPointF(0, 0), size);
        for (int sound = 0; sound < honestNames.size(); ++sound) {
            const QString buttonName = QStringLiteral("pitchedSoundButton-%1").arg(sound);
            const QString markName = QStringLiteral("pitchedSelectionMark-%1").arg(sound);
            QQuickItem *button = findQuickItem(window->contentItem(), buttonName);
            QQuickItem *selectionMark = findQuickItem(window->contentItem(), markName);
            QVERIFY2(button != nullptr, qPrintable(buttonName));
            QVERIFY2(selectionMark != nullptr, qPrintable(markName));
            QVERIFY(clickQuickItem(window, buttonName));
            QVERIFY(selectionMark->isVisible());
            QCOMPARE(selectionMark->property("text").toString(), QStringLiteral("✓"));
            QVERIFY(windowRect.contains(button->mapRectToScene(button->boundingRect())));
            QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(button);
            QVERIFY(accessible != nullptr);
            QCOMPARE(accessible->text(QAccessible::Name), honestNames.at(sound));
            QCOMPARE(accessible->state().checked, true);
        }
    }
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
        {QStringLiteral("pitchedSoundButton-1"), QStringLiteral("Pluck")},
        {QStringLiteral("pitchedSoundButton-2"), QStringLiteral("Bell")},
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

void QmlSmokeTest::keepsEveryCompositionLaneVisibleAtSupportedWindowSizes()
{
    const QList<QSize> supportedSizes = {
        QSize(1180, 760),
        QSize(900, 621),
        QSize(900, 620),
    };
    for (const QSize &size : supportedSizes) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppController controller(directory.filePath("autosave.json"), false);
        QVERIFY(controller.placePitched(0, 0));
        QVERIFY(controller.placePitched(1, 6));
        controller.selectPercussion(0);
        QVERIFY(controller.placePercussion(2));
        controller.selectPercussion(1);
        QVERIFY(controller.placePercussion(3));

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("app", &controller);
        engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window != nullptr);
        window->resize(size);
        QCoreApplication::processEvents();
        QCOMPARE(window->size(), size);

        QQuickItem *timeline = findQuickItem(window->contentItem(), "timeline");
        QVERIFY(timeline != nullptr);
        const QRectF viewportRect = timeline->mapRectToScene(timeline->boundingRect());
        const QRect windowRect(QPoint(0, 0), size);

        QStringList laneTargets;
        for (int pitch = 0; pitch < 7; ++pitch) {
            laneTargets.append(QStringLiteral("pitchCellMouse-0-%1").arg(pitch));
        }
        for (int drum = 0; drum < 2; ++drum) {
            laneTargets.append(QStringLiteral("drumCellMouse-0-%1").arg(drum));
        }

        for (const QString &objectName : laneTargets) {
            QQuickItem *target = findQuickItem(window->contentItem(), objectName);
            QVERIFY2(target != nullptr, qPrintable(objectName));
            const QRectF targetRect = target->mapRectToScene(target->boundingRect());
            const QPoint clickPoint = target->mapToScene(
                QPointF(target->width() / 2.0, target->height() / 2.0)).toPoint();
            QVERIFY2(viewportRect.contains(targetRect),
                     qPrintable(QStringLiteral("%1 is clipped at %2x%3")
                                    .arg(objectName).arg(size.width()).arg(size.height())));
            QVERIFY2(windowRect.contains(clickPoint),
                     qPrintable(QStringLiteral("%1 click point is outside the window")
                                    .arg(objectName)));
            QVERIFY2(targetRect.width() >= 44.0 && targetRect.height() >= 44.0,
                     qPrintable(QStringLiteral("%1 is too small for a child pointer target")
                                    .arg(objectName)));
        }

        const QList<QPair<QString, QString>> tokenCells = {
            {QStringLiteral("compositionToken-0-0"), QStringLiteral("pitchCellMouse-0-0")},
            {QStringLiteral("compositionToken-1-6"), QStringLiteral("pitchCellMouse-1-6")},
            {QStringLiteral("compositionToken-2-0"), QStringLiteral("drumCellMouse-2-0")},
            {QStringLiteral("compositionToken-3-1"), QStringLiteral("drumCellMouse-3-1")},
        };
        for (const auto &[tokenName, cellName] : tokenCells) {
            QQuickItem *token = findQuickItem(window->contentItem(), tokenName);
            QQuickItem *cell = findQuickItem(window->contentItem(), cellName);
            QVERIFY2(token != nullptr, qPrintable(tokenName));
            QVERIFY2(cell != nullptr, qPrintable(cellName));
            const QRectF tokenRect = token->mapRectToScene(token->boundingRect());
            const QRectF cellRect = cell->mapRectToScene(cell->boundingRect());
            QVERIFY2(cellRect.contains(tokenRect),
                     qPrintable(QStringLiteral("%1 [%2,%3 %4x%5] crosses %6 [%7,%8 %9x%10] at %11x%12")
                                    .arg(tokenName)
                                    .arg(tokenRect.x()).arg(tokenRect.y())
                                    .arg(tokenRect.width()).arg(tokenRect.height())
                                    .arg(cellName)
                                    .arg(cellRect.x()).arg(cellRect.y())
                                    .arg(cellRect.width()).arg(cellRect.height())
                                    .arg(size.width()).arg(size.height())));
            QVERIFY2(viewportRect.contains(tokenRect),
                     qPrintable(QStringLiteral("%1 is clipped at %2x%3")
                                    .arg(tokenName).arg(size.width()).arg(size.height())));
        }

        const QString screenshotDirectory = qEnvironmentVariable("CAIRN_SCREENSHOT_DIR");
        if (!screenshotDirectory.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshotDirectory));
            const QString path = QStringLiteral("%1/composition-lanes-%2x%3.png")
                                     .arg(screenshotDirectory)
                                     .arg(size.width())
                                     .arg(size.height());
            QVERIFY2(window->grabWindow().save(path), qPrintable(path));
        }
    }
}

void QmlSmokeTest::guidesSelectedDrumToItsVisibleRowThroughPointerPaths()
{
    const QList<QSize> supportedSizes = {QSize(1180, 760), QSize(900, 620)};
    for (const QSize &size : supportedSizes) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppController controller(directory.filePath("autosave.json"), false);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("app", &controller);
        engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window != nullptr);
        window->resize(size);
        QCoreApplication::processEvents();

        QQuickItem *timeline = findQuickItem(window->contentItem(), "timeline");
        QQuickItem *pitchTarget = findQuickItem(window->contentItem(), "pitchCellMouse-0-3");
        QQuickItem *wrongDrumTarget = findQuickItem(window->contentItem(), "drumCellMouse-0-1");
        QQuickItem *correctDrumTarget = findQuickItem(window->contentItem(), "drumCellMouse-0-0");
        QQuickItem *feedback = findQuickItem(window->contentItem(), "placementFeedback");
        QVERIFY(timeline != nullptr);
        QVERIFY(pitchTarget != nullptr);
        QVERIFY(wrongDrumTarget != nullptr);
        QVERIFY(correctDrumTarget != nullptr);
        QVERIFY(feedback != nullptr);

        const QRectF viewportRect = timeline->mapRectToScene(timeline->boundingRect());
        const QRect windowRect(QPoint(0, 0), size);
        for (QQuickItem *target : {pitchTarget, wrongDrumTarget, correctDrumTarget}) {
            const QRectF targetRect = target->mapRectToScene(target->boundingRect());
            const QPoint clickPoint = target->mapToScene(
                QPointF(target->width() / 2.0, target->height() / 2.0)).toPoint();
            QVERIFY2(viewportRect.contains(targetRect), qPrintable(target->objectName()));
            QVERIFY2(windowRect.contains(clickPoint), qPrintable(target->objectName()));
        }

        QVERIFY(clickQuickItem(window, QStringLiteral("percussionSoundButton-0")));
        QCOMPARE(controller.selectedKind(), QStringLiteral("percussion"));
        QCOMPARE(controller.selectedSound(), 0);

        QVERIFY(clickQuickItem(window, QStringLiteral("pitchCellMouse-0-3")));
        QVERIFY(feedback->isVisible());
        QAccessibleInterface *accessible = QAccessible::queryAccessibleInterface(feedback);
        QVERIFY(accessible != nullptr);
        QCOMPARE(accessible->text(QAccessible::Name),
                 QStringLiteral("Put Thump in the Thump drum row."));
        QCOMPARE(controller.composition()->rowCount(), 0);

        QVERIFY(clickQuickItem(window, QStringLiteral("drumCellMouse-0-1")));
        QVERIFY(feedback->isVisible());
        QCOMPARE(accessible->text(QAccessible::Name),
                 QStringLiteral("Put Thump in the Thump drum row."));
        QCOMPARE(controller.composition()->rowCount(), 0);

        QVERIFY(clickQuickItem(window, QStringLiteral("drumCellMouse-0-0")));
        QCOMPARE(controller.composition()->rowCount(), 1);
        QVERIFY(!feedback->isVisible());

        QVERIFY(clickQuickItem(window, QStringLiteral("pitchCellMouse-1-3")));
        QVERIFY(feedback->isVisible());
        QTRY_VERIFY_WITH_TIMEOUT(!feedback->isVisible(), 2600);
        QCOMPARE(controller.composition()->rowCount(), 1);
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

void QmlSmokeTest::keepsPlacementFeedbackFromMovingWorkspace()
{
    const QList<QSize> supportedSizes = {QSize(1180, 760), QSize(900, 620)};
    for (const QSize &size : supportedSizes) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        AppController controller(directory.filePath("autosave.json"), false);

        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("app", &controller);
        engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window != nullptr);
        window->resize(size);
        QCoreApplication::processEvents();

        QQuickItem *pitchGrid = findQuickItem(window->contentItem(), "pitchGrid");
        QQuickItem *drumLane = findQuickItem(window->contentItem(), "drumLane");
        QQuickItem *palette = findQuickItem(window->contentItem(), "pitchedSoundButton-0");
        QQuickItem *destination = findQuickItem(window->contentItem(), "pitchCellMouse-0-3");
        QQuickItem *playButton = findQuickItem(window->contentItem(), "playButton");
        QObject *feedback = window->findChild<QObject *>("placementFeedback");
        QVERIFY(pitchGrid != nullptr);
        QVERIFY(drumLane != nullptr);
        QVERIFY(palette != nullptr);
        QVERIFY(destination != nullptr);
        QVERIFY(playButton != nullptr);
        QVERIFY(feedback != nullptr);

        const QRectF windowRect(QPointF(0, 0), size);
        const QRectF destinationRect = destination->mapRectToScene(destination->boundingRect());
        const QPoint destinationPoint = destination->mapToScene(
            QPointF(destination->width() / 2.0, destination->height() / 2.0)).toPoint();
        QVERIFY2(windowRect.contains(destinationRect), "Destination bounds must be visible");
        QVERIFY2(QRect(QPoint(0, 0), size).contains(destinationPoint),
                 "Destination click point must be inside the window");

        const QList<QQuickItem *> stableItems = {pitchGrid, drumLane, palette};
        QList<QPointF> before;
        for (QQuickItem *item : stableItems) {
            before.append(item->mapToScene(QPointF()));
        }

        playButton->forceActiveFocus(Qt::TabFocusReason);
        QVERIFY(playButton->hasActiveFocus());
        bool placed = false;
        for (int pitch = 0; pitch < 3; ++pitch) {
            QVERIFY(invokePitchedPlacement(window, 0, pitch, placed));
            QVERIFY(placed);
        }
        QVERIFY(invokePitchedPlacement(window, 0, 3, placed));
        QVERIFY(!placed);
        QCoreApplication::processEvents();
        QVERIFY(feedback->property("visible").toBool());
        QVERIFY(playButton->hasActiveFocus());

        for (qsizetype index = 0; index < stableItems.size(); ++index) {
            QCOMPARE(stableItems.at(index)->mapToScene(QPointF()), before.at(index));
        }

        QTRY_VERIFY_WITH_TIMEOUT(!feedback->property("visible").toBool(), 2600);
        QVERIFY(playButton->hasActiveFocus());
        for (qsizetype index = 0; index < stableItems.size(); ++index) {
            QCOMPARE(stableItems.at(index)->mapToScene(QPointF()), before.at(index));
        }
    }
}

void QmlSmokeTest::keepsConcurrentNotificationsStableAndReadable()
{
    const QList<QSize> supportedSizes = {QSize(1180, 760), QSize(900, 620)};
    for (const QSize &size : supportedSizes) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString blockedParent = directory.filePath("blocked");
        QFile blocker(blockedParent);
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        QVERIFY(blocker.write("not a directory") > 0);
        blocker.close();

        auto audio = std::make_unique<AudioEngine>(false);
        AppController controller(blockedParent + QStringLiteral("/autosave.json"), true,
                                 std::move(audio));
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("app", &controller);
        engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window != nullptr);
        window->resize(size);
        QCoreApplication::processEvents();

        QQuickItem *pitchGrid = findQuickItem(window->contentItem(), "pitchGrid");
        QQuickItem *drumLane = findQuickItem(window->contentItem(), "drumLane");
        QQuickItem *palette = findQuickItem(window->contentItem(), "pitchedSoundButton-0");
        QQuickItem *timeline = findQuickItem(window->contentItem(), "timeline");
        QQuickItem *destination = findQuickItem(window->contentItem(), "pitchCellMouse-0-3");
        QQuickItem *playButton = findQuickItem(window->contentItem(), "playButton");
        QQuickItem *saveBanner = findQuickItem(window->contentItem(), "saveFailureBanner");
        QQuickItem *audioBanner = findQuickItem(window->contentItem(), "audioFailureBanner");
        QQuickItem *placementBanner = findQuickItem(window->contentItem(), "placementFeedback");
        QQuickItem *saveLabel = findQuickItem(window->contentItem(), "saveFailureLabel");
        QQuickItem *audioLabel = findQuickItem(window->contentItem(), "audioFailureLabel");
        QQuickItem *placementLabel = findQuickItem(window->contentItem(), "placementFeedbackLabel");
        QVERIFY(pitchGrid != nullptr);
        QVERIFY(drumLane != nullptr);
        QVERIFY(palette != nullptr);
        QVERIFY(timeline != nullptr);
        QVERIFY(destination != nullptr);
        QVERIFY(playButton != nullptr);
        QVERIFY(saveBanner != nullptr);
        QVERIFY(audioBanner != nullptr);
        QVERIFY(placementBanner != nullptr);
        QVERIFY(saveLabel != nullptr);
        QVERIFY(audioLabel != nullptr);
        QVERIFY(placementLabel != nullptr);

        const QList<QQuickItem *> stableItems = {timeline, pitchGrid, drumLane, palette};
        QList<QPointF> before;
        for (QQuickItem *item : stableItems) {
            before.append(item->mapToScene(QPointF()));
        }
        const auto verifyStable = [&stableItems, &before]() {
            for (qsizetype index = 0; index < stableItems.size(); ++index) {
                QCOMPARE(stableItems.at(index)->mapToScene(QPointF()), before.at(index));
            }
        };

        playButton->forceActiveFocus(Qt::TabFocusReason);
        QVERIFY(playButton->hasActiveFocus());
        bool placed = true;
        for (int pitch = 0; pitch < 3; ++pitch) {
            QVERIFY(invokePitchedPlacement(window, 0, pitch, placed));
            QVERIFY(!placed);
        }
        QVERIFY(controller.saveFailed());
        QVERIFY(saveBanner->isVisible());
        verifyStable();

        controller.play();
        QCoreApplication::processEvents();
        QVERIFY(controller.audioFailed());
        QVERIFY(audioBanner->isVisible());
        verifyStable();

        QVERIFY(invokePitchedPlacement(window, 0, 3, placed));
        QVERIFY(!placed);
        QCoreApplication::processEvents();
        QVERIFY(placementBanner->isVisible());
        QVERIFY(playButton->hasActiveFocus());
        verifyStable();

        const QRectF windowRect(QPointF(0, 0), size);
        const QList<QQuickItem *> banners = {saveBanner, audioBanner, placementBanner};
        for (QQuickItem *banner : banners) {
            QTRY_VERIFY_WITH_TIMEOUT(banner->width() > 0.0 && banner->height() > 0.0, 250);
        }
        QTRY_VERIFY_WITH_TIMEOUT(
            saveBanner->mapRectToScene(saveBanner->boundingRect()).bottom()
                < audioBanner->mapRectToScene(audioBanner->boundingRect()).top(), 250);
        QTRY_VERIFY_WITH_TIMEOUT(
            audioBanner->mapRectToScene(audioBanner->boundingRect()).bottom()
                < placementBanner->mapRectToScene(placementBanner->boundingRect()).top(), 250);
        QList<QRectF> bannerRects;
        for (QQuickItem *banner : banners) {
            const QRectF rect = banner->mapRectToScene(banner->boundingRect());
            QVERIFY2(windowRect.contains(rect),
                     qPrintable(QStringLiteral("Notification %1 bounds %2,%3 %4x%5 exceed %6x%7")
                                    .arg(banner->objectName()).arg(rect.x()).arg(rect.y())
                                    .arg(rect.width()).arg(rect.height())
                                    .arg(size.width()).arg(size.height())));
            QVERIFY2(rect.height() >= 28.0, "Notification must remain readable");
            bannerRects.append(rect);
        }
        QVERIFY2(bannerRects.at(0).bottom() < bannerRects.at(1).top(),
                 "Save must appear above audio without overlap");
        QVERIFY2(bannerRects.at(1).bottom() < bannerRects.at(2).top(),
                 "Audio must appear above placement without overlap");

        const QList<QQuickItem *> labels = {saveLabel, audioLabel, placementLabel};
        for (qsizetype index = 0; index < labels.size(); ++index) {
            QQuickItem *label = labels.at(index);
            const QRectF labelRect = label->mapRectToScene(label->boundingRect());
            QVERIFY2(bannerRects.at(index).contains(labelRect),
                     qPrintable(label->objectName()));
            const QFont font = label->property("font").value<QFont>();
            QVERIFY2(font.pixelSize() >= 14, qPrintable(label->objectName()));
        }

        const QRectF playRect = playButton->mapRectToScene(playButton->boundingRect());
        const QRectF destinationRect = destination->mapRectToScene(destination->boundingRect());
        const QPoint playPoint = playButton->mapToScene(
            QPointF(playButton->width() / 2.0, playButton->height() / 2.0)).toPoint();
        const QPoint destinationPoint = destination->mapToScene(
            QPointF(destination->width() / 2.0, destination->height() / 2.0)).toPoint();
        QVERIFY(windowRect.contains(playRect));
        QVERIFY(windowRect.contains(destinationRect));
        QVERIFY(QRect(QPoint(0, 0), size).contains(playPoint));
        QVERIFY(QRect(QPoint(0, 0), size).contains(destinationPoint));
        for (const QRectF &bannerRect : bannerRects) {
            QVERIFY(!bannerRect.intersects(playRect));
            QVERIFY(!bannerRect.intersects(destinationRect));
        }

        const QStringList paletteButtons = {
            QStringLiteral("pitchedSoundButton-0"),
            QStringLiteral("pitchedSoundButton-1"),
            QStringLiteral("pitchedSoundButton-2"),
            QStringLiteral("pitchedSoundButton-3"),
            QStringLiteral("percussionSoundButton-0"),
            QStringLiteral("percussionSoundButton-1"),
        };
        for (const QString &objectName : paletteButtons) {
            QQuickItem *button = findQuickItem(window->contentItem(), objectName);
            QVERIFY2(button != nullptr, qPrintable(objectName));
            const QRectF buttonRect = button->mapRectToScene(button->boundingRect());
            const QPoint buttonPoint = button->mapToScene(
                QPointF(button->width() / 2.0, button->height() / 2.0)).toPoint();
            QVERIFY2(windowRect.contains(buttonRect), qPrintable(objectName));
            QVERIFY2(QRect(QPoint(0, 0), size).contains(buttonPoint),
                     qPrintable(objectName));
            QVERIFY2(buttonRect.width() >= 44.0 && buttonRect.height() >= 44.0,
                     qPrintable(objectName));
        }

        const QString screenshotDirectory = qEnvironmentVariable("CAIRN_SCREENSHOT_DIR");
        if (!screenshotDirectory.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshotDirectory));
            const QString path = QStringLiteral("%1/notifications-%2x%3.png")
                                     .arg(screenshotDirectory)
                                     .arg(size.width())
                                     .arg(size.height());
            QVERIFY2(window->grabWindow().save(path), qPrintable(path));
        }

        QTRY_VERIFY_WITH_TIMEOUT(!placementBanner->isVisible(), 2600);
        QVERIFY(saveBanner->isVisible());
        QVERIFY(audioBanner->isVisible());
        QVERIFY(playButton->hasActiveFocus());
        verifyStable();
    }
}

void QmlSmokeTest::wrapsLocalizedNotificationsWithoutObscuringWorkspace()
{
    LongMessageTranslator translator;
    ScopedTranslator scopedTranslator(&translator);
    const QList<QSize> supportedSizes = {QSize(1180, 760), QSize(900, 620)};

    for (const QSize &size : supportedSizes) {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString blockedParent = directory.filePath("blocked");
        QFile blocker(blockedParent);
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        QVERIFY(blocker.write("not a directory") > 0);
        blocker.close();

        auto audio = std::make_unique<AudioEngine>(false);
        AppController controller(blockedParent + QStringLiteral("/autosave.json"), true,
                                 std::move(audio));
        QQmlApplicationEngine engine;
        engine.rootContext()->setContextProperty("app", &controller);
        engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
        QCOMPARE(engine.rootObjects().size(), 1);
        auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
        QVERIFY(window != nullptr);
        window->resize(size);
        QCoreApplication::processEvents();

        QQuickItem *tray = findQuickItem(window->contentItem(), "notificationTray");
        QQuickItem *timeline = findQuickItem(window->contentItem(), "timeline");
        QQuickItem *pitchGrid = findQuickItem(window->contentItem(), "pitchGrid");
        QQuickItem *drumLane = findQuickItem(window->contentItem(), "drumLane");
        QQuickItem *palette = findQuickItem(window->contentItem(), "pitchedSoundButton-0");
        QQuickItem *drumControl = findQuickItem(window->contentItem(), "percussionSoundButton-0");
        QQuickItem *playButton = findQuickItem(window->contentItem(), "playButton");
        QVERIFY(tray != nullptr);
        QVERIFY(timeline != nullptr);
        QVERIFY(pitchGrid != nullptr);
        QVERIFY(drumLane != nullptr);
        QVERIFY(palette != nullptr);
        QVERIFY(drumControl != nullptr);
        QVERIFY(playButton != nullptr);

        const QList<QQuickItem *> stableItems = {timeline, pitchGrid, drumLane,
                                                 palette, drumControl};
        QList<QRectF> stableRects;
        for (QQuickItem *item : stableItems) {
            stableRects.append(item->mapRectToScene(item->boundingRect()));
        }

        playButton->forceActiveFocus(Qt::TabFocusReason);
        QVERIFY(playButton->hasActiveFocus());
        bool placed = true;
        for (int pitch = 0; pitch < 3; ++pitch) {
            QVERIFY(invokePitchedPlacement(window, 0, pitch, placed));
            QVERIFY(!placed);
        }
        controller.play();
        QVERIFY(invokePitchedPlacement(window, 0, 3, placed));
        QVERIFY(!placed);
        QCoreApplication::processEvents();

        QVERIFY(controller.saveFailed());
        QVERIFY(controller.audioFailed());
        QVERIFY(playButton->hasActiveFocus());
        for (qsizetype index = 0; index < stableItems.size(); ++index) {
            QCOMPARE(stableItems.at(index)->mapRectToScene(stableItems.at(index)->boundingRect()),
                     stableRects.at(index));
        }

        const QRectF windowRect(QPointF(0, 0), size);
        const QRectF trayRect = tray->mapRectToScene(tray->boundingRect());
        QVERIFY2(windowRect.contains(trayRect), "Localized notification tray must stay in window");

        const QStringList bannerNames = {
            QStringLiteral("saveFailureBanner"),
            QStringLiteral("audioFailureBanner"),
            QStringLiteral("placementFeedback"),
        };
        const QStringList labelNames = {
            QStringLiteral("saveFailureLabel"),
            QStringLiteral("audioFailureLabel"),
            QStringLiteral("placementFeedbackLabel"),
        };
        QList<QRectF> bannerRects;
        for (qsizetype index = 0; index < bannerNames.size(); ++index) {
            QQuickItem *banner = findQuickItem(window->contentItem(), bannerNames.at(index));
            QQuickItem *label = findQuickItem(window->contentItem(), labelNames.at(index));
            QVERIFY2(banner != nullptr, qPrintable(bannerNames.at(index)));
            QVERIFY2(label != nullptr, qPrintable(labelNames.at(index)));
            QVERIFY2(banner->isVisible(), qPrintable(bannerNames.at(index)));
            if (size.width() == 900) {
                QVERIFY2(label->property("lineCount").toInt() >= 2,
                         qPrintable(QStringLiteral("%1 did not wrap").arg(labelNames.at(index))));
            }

            const QRectF bannerRect = banner->mapRectToScene(banner->boundingRect());
            const QRectF labelRect = label->mapRectToScene(label->boundingRect());
            QVERIFY2(windowRect.contains(bannerRect), qPrintable(bannerNames.at(index)));
            QVERIFY2(bannerRect.contains(labelRect), qPrintable(labelNames.at(index)));
            QVERIFY2(label->property("paintedWidth").toReal() <= label->width() + 0.5,
                     qPrintable(labelNames.at(index)));
            QVERIFY2(label->property("paintedHeight").toReal() <= label->height() + 0.5,
                     qPrintable(labelNames.at(index)));
            bannerRects.append(bannerRect);
        }
        QVERIFY2(bannerRects.at(0).bottom() < bannerRects.at(1).top(),
                 "Save notification must remain first without overlap");
        QVERIFY2(bannerRects.at(1).bottom() < bannerRects.at(2).top(),
                 "Audio notification must remain second without overlap");

        QStringList protectedNames = {
            QStringLiteral("undoButton"),
            QStringLiteral("eraserButton"),
            QStringLiteral("playButton"),
            QStringLiteral("loopCheckBox"),
            QStringLiteral("clearSongButton"),
            QStringLiteral("pitchedSoundButton-0"),
            QStringLiteral("pitchedSoundButton-1"),
            QStringLiteral("pitchedSoundButton-2"),
            QStringLiteral("pitchedSoundButton-3"),
            QStringLiteral("percussionSoundButton-0"),
            QStringLiteral("percussionSoundButton-1"),
            QStringLiteral("compositionToken-0-0"),
            QStringLiteral("compositionToken-0-1"),
            QStringLiteral("compositionToken-0-2"),
        };
        for (const QString &objectName : protectedNames) {
            QQuickItem *item = findQuickItem(window->contentItem(), objectName);
            QVERIFY2(item != nullptr, qPrintable(objectName));
            const QRectF itemRect = item->mapRectToScene(item->boundingRect());
            QVERIFY2(windowRect.contains(itemRect), qPrintable(objectName));
            for (const QRectF &bannerRect : bannerRects) {
                QVERIFY2(!bannerRect.intersects(itemRect),
                         qPrintable(QStringLiteral("Notification %1,%2 %3x%4 covers %5 at %6,%7 %8x%9")
                                        .arg(bannerRect.x()).arg(bannerRect.y())
                                        .arg(bannerRect.width()).arg(bannerRect.height())
                                        .arg(objectName).arg(itemRect.x()).arg(itemRect.y())
                                        .arg(itemRect.width()).arg(itemRect.height())));
            }
        }

        const QString screenshotDirectory = qEnvironmentVariable("CAIRN_SCREENSHOT_DIR");
        if (!screenshotDirectory.isEmpty()) {
            QVERIFY(QDir().mkpath(screenshotDirectory));
            const QString path = QStringLiteral("%1/localized-notifications-%2x%3.png")
                                     .arg(screenshotDirectory)
                                     .arg(size.width())
                                     .arg(size.height());
            QVERIFY2(window->grabWindow().save(path), qPrintable(path));
        }
    }
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

void QmlSmokeTest::preservesAnimatingTokenDelegatesAcrossUnrelatedEdits()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    AppController controller(directory.filePath("autosave.json"), false);
    QVERIFY(controller.addMeasure());
    controller.selectPitched(1);
    QVERIFY(controller.placePitched(0, 2));
    controller.selectPercussion(0);
    QVERIFY(controller.placePercussion(1));
    controller.selectPitched(3);
    QVERIFY(controller.placePitched(8, 5));

    const QString pitchedId = controller.composition()->data(
        controller.composition()->index(0), CompositionModel::IdRole).toString();
    const QString percussionId = controller.composition()->data(
        controller.composition()->index(1), CompositionModel::IdRole).toString();
    const QJsonObject beforeTemporaryEdit = controller.composition()->toJson();
    controller.selectPitched(2);
    QVERIFY(controller.placePitched(2, 4));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("app", &controller);
    engine.load(QUrl::fromLocalFile(QStringLiteral(CAIRN_MUSIC_QML_PATH)));
    QCOMPARE(engine.rootObjects().size(), 1);
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    QVERIFY(window != nullptr);
    window->resize(1180, 760);
    QCoreApplication::processEvents();

    QQuickItem *pitchedToken = findQuickItem(window->contentItem(), "compositionToken-0-2");
    QQuickItem *percussionToken = findQuickItem(window->contentItem(), "compositionToken-1-0");
    QQuickItem *removedToken = findQuickItem(window->contentItem(), "compositionToken-8-5");
    QVERIFY(pitchedToken != nullptr);
    QVERIFY(percussionToken != nullptr);
    QVERIFY(removedToken != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(pitchedToken->property("popScale").toReal() > 1.0, 100);
    QVERIFY(pitchedToken->property("popScale").toReal() < 1.18);
    QVERIFY(percussionToken->property("popScale").toReal() > 1.0);
    QPointer<QQuickItem> pitchedIdentity(pitchedToken);
    QPointer<QQuickItem> percussionIdentity(percussionToken);
    QPointer<QQuickItem> removedIdentity(removedToken);

    auto clickVisibleControl = [window](QQuickItem *control) {
        QVERIFY(control != nullptr);
        QVERIFY(control->isVisible());
        const QRectF sceneBounds = control->mapRectToScene(control->boundingRect());
        const QRectF visibleWindow(QPointF(0, 0), QSizeF(window->width(), window->height()));
        QVERIFY2(visibleWindow.contains(sceneBounds),
                 "mapped target bounds must remain inside the visible window");
        const QPointF scenePoint = control->mapToScene(control->boundingRect().center());
        QVERIFY(sceneBounds.contains(scenePoint));
        QVERIFY(visibleWindow.contains(scenePoint));
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, scenePoint.toPoint());
        QCoreApplication::processEvents();
    };

    clickVisibleControl(findQuickItem(window->contentItem(), "undoButton"));
    QCOMPARE(controller.composition()->toJson(), beforeTemporaryEdit);
    QVERIFY(!pitchedIdentity.isNull());
    QVERIFY(!percussionIdentity.isNull());
    QCOMPARE(findQuickItem(window->contentItem(), "compositionToken-0-2"), pitchedIdentity.data());
    QCOMPARE(findQuickItem(window->contentItem(), "compositionToken-1-0"), percussionIdentity.data());
    QCOMPARE(controller.composition()->data(
                 controller.composition()->index(0), CompositionModel::IdRole).toString(), pitchedId);
    QCOMPARE(controller.composition()->data(
                 controller.composition()->index(1), CompositionModel::IdRole).toString(), percussionId);

    const QJsonObject beforeRemoval = controller.composition()->toJson();
    QVERIFY(pitchedIdentity->property("popScale").toReal() > 1.0);
    clickVisibleControl(findQuickItem(window->contentItem(), "removeMeasureButton"));
    QVERIFY(!pitchedIdentity.isNull());
    QVERIFY(!percussionIdentity.isNull());
    QVERIFY(removedIdentity.isNull());
    QCOMPARE(findQuickItem(window->contentItem(), "compositionToken-0-2"), pitchedIdentity.data());
    QCOMPARE(findQuickItem(window->contentItem(), "compositionToken-1-0"), percussionIdentity.data());

    clickVisibleControl(findQuickItem(window->contentItem(), "undoButton"));
    QCOMPARE(controller.composition()->toJson(), beforeRemoval);
    QVERIFY(!pitchedIdentity.isNull());
    QVERIFY(!percussionIdentity.isNull());
    QCOMPARE(findQuickItem(window->contentItem(), "compositionToken-0-2"), pitchedIdentity.data());
    QCOMPARE(findQuickItem(window->contentItem(), "compositionToken-1-0"), percussionIdentity.data());
    QVERIFY(findQuickItem(window->contentItem(), "compositionToken-8-5") != nullptr);
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
