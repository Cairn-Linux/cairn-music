#include <QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <memory>

#include "AppController.h"
#include "AudioEngine.h"
#include "CompositionModel.h"

class FakeAudioEngine final : public AudioEngine
{
public:
    FakeAudioEngine()
        : AudioEngine(false)
    {
    }

    void finishCurrentBuffer()
    {
        m_outputState = QAudio::IdleState;
        handleState(m_outputState);
    }

    void failWithOutputError()
    {
        m_outputError = QAudio::IOError;
        m_outputState = QAudio::StoppedState;
        handleState(m_outputState);
    }

    void failWhileIdle()
    {
        m_outputError = QAudio::IOError;
        m_outputState = QAudio::IdleState;
        handleState(m_outputState);
    }

    void notifyStoppedState()
    {
        m_outputError = QAudio::NoError;
        m_outputState = QAudio::StoppedState;
        handleState(m_outputState);
    }

    [[nodiscard]] bool resourcesReleased() const noexcept
    {
        return !activeBufferOpen() && bufferedByteCount() == 0;
    }

    int startCalls = 0;
    int stopCalls = 0;
    bool startSucceeds = true;

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
        if (startSucceeds) {
            m_outputError = QAudio::NoError;
            m_outputState = QAudio::ActiveState;
            return;
        }
        m_outputError = QAudio::OpenError;
        m_outputState = QAudio::StoppedState;
        handleState(m_outputState);
    }

    void stopOutput() override
    {
        ++stopCalls;
        m_outputError = QAudio::NoError;
        m_outputState = QAudio::StoppedState;
        handleState(m_outputState);
    }

private:
    QAudio::Error m_outputError = QAudio::NoError;
    QAudio::State m_outputState = QAudio::StoppedState;
};

class ManualPlaybackClock final : public PlaybackClock
{
public:
    void restart() override { m_elapsed = 0; }
    [[nodiscard]] qint64 elapsed() const noexcept override { return m_elapsed; }
    void setElapsed(qint64 elapsed) { m_elapsed = elapsed; }

private:
    qint64 m_elapsed = 0;
};

class AppControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void disabledAudioDoesNotReportUnavailableDevice();
    void placesSelectedSoundAndReopensAutosave();
    void preservesMalformedAutosaveUntilNewSongIsConfirmed();
    void reportsAutosaveFailureWhileKeepingAcceptedEditsInMemory();
    void distinguishesPolyphonyRejectionFromSaveFailure();
    void pitchedLimitRejectionKeepsCompositionPlaybackAndIndicators();
    void unavailableCommandsKeepCompositionPlaybackAndIndicators();
    void turnsLoopOnDuringPlayback();
    void turnsLoopOffDuringPlayback();
    void stopDoesNotRestartLoopingPlayback();
    void playOnceStopsAtNaturalCompletion();
    void previewInterruptsCompositionLoopPolicy();
    void previewActivityIsSeparateFromCompositionPlayback();
    void undoDuringPlaybackStopsAndClearsQueuedAudio();
    void pitchedPlacementAndReplacementStopBeforeMutationThenPreview();
    void percussionPlacementAndExistingTapPreview();
    void unchangedPitchedPlacementPreviewsWithoutSavingOrAddingUndo();
    void unchangedPitchedPlacementKeepsCompositionPlaybackAndIndicators();
    void unchangedPercussionPlacementDoesNotRetryFailedSave();
    void unchangedPercussionPlacementKeepsCompositionPlaybackAndIndicators();
    void emptyEraserClicksKeepCompositionPlaybackAndIndicators();
    void eraserStopsPlaybackBeforeMutation();
    void addMeasureStopsPlaybackBeforeMutation();
    void removeMeasurePersistsAndUndoRestoresExactEvents();
    void removeMeasureStopsPlaybackBeforeMutation();
    void clearSongPersistsAndUndoRestoresExactComposition();
    void alreadyClearSongKeepsCompositionPlaybackAndIndicators();
    void clearSongStopsPlaybackAndPreviewBeforeMutation();
    void clearSongReportsSaveFailureWithoutLyingAboutMemoryState();
    void recoversFromOutputError();
    void idleOutputErrorStopsInsteadOfRestartingLoop();
    void failedLoopRestartBecomesRecoverableError();
    void repeatedPlayStopCyclesStayConsistent();
    void exposesPlaybackStepAndSemanticStatus();
    void reportsLoopRestart();
};

void AppControllerTest::disabledAudioDoesNotReportUnavailableDevice()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    AppController controller(directory.filePath("autosave.json"), false);

    QVERIFY(!controller.audioFailed());
    QVERIFY(!controller.playing());

    AppController nullInjectedController(
        directory.filePath("second-autosave.json"), false,
        std::unique_ptr<AudioEngine>{});
    QVERIFY(!nullInjectedController.audioFailed());
    QVERIFY(!nullInjectedController.playing());
}

void AppControllerTest::placesSelectedSoundAndReopensAutosave()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath("autosave.json");

    {
        AppController controller(path, false);
        controller.selectPitched(3);
        QVERIFY(controller.placePitched(1, 4));
        QCOMPARE(controller.composition()->rowCount(), 1);
        QCOMPARE(controller.composition()->data(
                     controller.composition()->index(0), CompositionModel::SoundIdRole).toInt(), 3);
    }

    AppController reopened(path, false);
    QCOMPARE(reopened.composition()->rowCount(), 1);
    QCOMPARE(reopened.composition()->data(
                 reopened.composition()->index(0), CompositionModel::StepRole).toInt(), 1);
}

void AppControllerTest::preservesMalformedAutosaveUntilNewSongIsConfirmed()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath("autosave.json");
    const QByteArray malformed = "{not valid json";
    QFile autosave(path);
    QVERIFY(autosave.open(QIODevice::WriteOnly));
    QCOMPARE(autosave.write(malformed), malformed.size());
    autosave.close();

    AppController controller(path, false);
    QVERIFY(controller.loadFailed());
    QCOMPARE(controller.loadFailureMessage(),
             QStringLiteral("This song needs help. Ask a grown-up."));
    QVERIFY(!controller.loadFailureMessage().contains(path));
    QCOMPARE(controller.composition()->metaObject()->indexOfMethod("addMeasure()"), -1);

    // Even trusted C++ code holding the model pointer cannot influence the
    // composition created by the explicit recovery action.
    QVERIFY(controller.composition()->addMeasure());
    QVERIFY(controller.composition()->placePitched(0, 0, 0));

    controller.selectPitched(2);
    QVERIFY(!controller.placePitched(0, 3));
    QVERIFY(autosave.open(QIODevice::ReadOnly));
    QCOMPARE(autosave.readAll(), malformed);
    autosave.close();

    QVERIFY(controller.preserveFailedAutosaveAndStartNew());
    QVERIFY(!controller.loadFailed());
    QCOMPARE(controller.composition()->measureCount(), 2);
    QCOMPARE(controller.composition()->rowCount(), 0);

    const QStringList recoveryFiles = QDir(directory.path()).entryList(
        {QStringLiteral("autosave.json.recovery-*")}, QDir::Files);
    QCOMPARE(recoveryFiles.size(), 1);
    QFile recovery(directory.filePath(recoveryFiles.constFirst()));
    QVERIFY(recovery.open(QIODevice::ReadOnly));
    QCOMPARE(recovery.readAll(), malformed);

    QVERIFY(controller.placePitched(0, 3));
}

void AppControllerTest::reportsAutosaveFailureWhileKeepingAcceptedEditsInMemory()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString blockedParent = directory.filePath("blocked");
    QFile blocker(blockedParent);
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    QVERIFY(blocker.write("not a directory") > 0);
    blocker.close();
    const QString path = blockedParent + QStringLiteral("/autosave.json");

    AppController controller(path, false);
    controller.selectPitched(1);

    QVERIFY(!controller.placePitched(0, 2));
    QCOMPARE(controller.composition()->rowCount(), 1);
    QVERIFY(controller.saveFailed());
    QCOMPARE(controller.saveFailureMessage(),
             QStringLiteral("Your song is here, but it is not saved yet."));
    QVERIFY(!controller.saveFailureMessage().contains(path));

    QVERIFY(!controller.eraseAt("pitched", 0, 2));
    QCOMPARE(controller.composition()->rowCount(), 0);
    QVERIFY(controller.saveFailed());

    QVERIFY(!controller.undo());
    QCOMPARE(controller.composition()->rowCount(), 1);
    QVERIFY(controller.saveFailed());

    QVERIFY(!controller.addMeasure());
    QCOMPARE(controller.composition()->measureCount(), 3);
    QVERIFY(controller.saveFailed());

    QVERIFY(QFile::remove(blockedParent));
    QVERIFY(QDir().mkpath(blockedParent));
    QVERIFY(controller.addMeasure());
    QCOMPARE(controller.composition()->measureCount(), 4);
    QVERIFY(!controller.saveFailed());
    QVERIFY(controller.saveFailureMessage().isEmpty());

    AppController reopened(path, false);
    QCOMPARE(reopened.composition()->measureCount(), 4);
    QCOMPARE(reopened.composition()->rowCount(), 1);
}

void AppControllerTest::distinguishesPolyphonyRejectionFromSaveFailure()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString blockedParent = directory.filePath("blocked");
    QFile blocker(blockedParent);
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    QVERIFY(blocker.write("not a directory") > 0);
    blocker.close();

    AppController controller(blockedParent + QStringLiteral("/autosave.json"), false);
    controller.selectPitched(0);

    for (int pitch = 0; pitch < 3; ++pitch) {
        QVERIFY(!controller.placePitched(0, pitch));
        QVERIFY(controller.saveFailed());
        QVERIFY(!controller.pitchedPlacementRejected());
    }
    QCOMPARE(controller.composition()->rowCount(), 3);

    QVERIFY(!controller.placePitched(0, 3));
    QVERIFY(controller.pitchedPlacementRejected());
    QCOMPARE(controller.composition()->rowCount(), 3);

    QVERIFY(controller.placePitched(0, 1));
    QVERIFY(!controller.pitchedPlacementRejected());
    QCOMPARE(controller.composition()->rowCount(), 3);
}

void AppControllerTest::pitchedLimitRejectionKeepsCompositionPlaybackAndIndicators()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));
    controller.selectPitched(0);
    for (int row = 0; row < 3; ++row) {
        QVERIFY(controller.placePitched(0, row));
    }
    controller.setLoopEnabled(true);
    controller.play();
    fakeAudio->finishCurrentBuffer();
    QCOMPARE(controller.playbackCycle(), 1);
    const int playbackStep = controller.playbackStep();
    const int startCalls = fakeAudio->startCalls;
    const int stopCalls = fakeAudio->stopCalls;

    QVERIFY(!controller.placePitched(0, 3));

    QVERIFY(controller.pitchedPlacementRejected());
    QCOMPARE(controller.composition()->rowCount(), 3);
    QVERIFY(controller.playing());
    QVERIFY(controller.compositionPlaying());
    QCOMPARE(fakeAudio->startCalls, startCalls);
    QCOMPARE(fakeAudio->stopCalls, stopCalls);
    QCOMPARE(controller.playbackStep(), playbackStep);
    QCOMPARE(controller.playbackCycle(), 1);
}

void AppControllerTest::unavailableCommandsKeepCompositionPlaybackAndIndicators()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));
    controller.play();
    const int playbackStep = controller.playbackStep();
    const int stopCalls = fakeAudio->stopCalls;

    QVERIFY(!controller.removeMeasure());
    QVERIFY(!controller.undo());
    QVERIFY(!controller.placePitched(-1, 0));
    QVERIFY(!controller.placePercussion(0));

    QVERIFY(controller.playing());
    QVERIFY(controller.compositionPlaying());
    QCOMPARE(fakeAudio->stopCalls, stopCalls);
    QCOMPARE(controller.playbackStep(), playbackStep);

    controller.stop();
    for (int measure = 2; measure < 8; ++measure) {
        QVERIFY(controller.addMeasure());
    }
    controller.play();
    const int maxMeasureStopCalls = fakeAudio->stopCalls;
    QVERIFY(!controller.addMeasure());
    QVERIFY(controller.playing());
    QVERIFY(controller.compositionPlaying());
    QCOMPARE(fakeAudio->stopCalls, maxMeasureStopCalls);
}

void AppControllerTest::turnsLoopOnDuringPlayback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));

    QVERIFY(!controller.loopEnabled());
    controller.play();
    QVERIFY(fakeAudio->playing());
    QVERIFY(!fakeAudio->loopEnabled());

    controller.setLoopEnabled(true);
    QVERIFY(controller.loopEnabled());
    QVERIFY(fakeAudio->loopEnabled());

    fakeAudio->finishCurrentBuffer();
    QVERIFY(fakeAudio->playing());
    QCOMPARE(fakeAudio->startCalls, 2);
    QVERIFY(!fakeAudio->resourcesReleased());
}

void AppControllerTest::turnsLoopOffDuringPlayback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));

    controller.setLoopEnabled(true);
    controller.play();
    QVERIFY(fakeAudio->playing());
    QVERIFY(fakeAudio->loopEnabled());

    controller.setLoopEnabled(false);
    QVERIFY(!controller.loopEnabled());
    QVERIFY(!fakeAudio->loopEnabled());

    fakeAudio->finishCurrentBuffer();
    QVERIFY(!fakeAudio->playing());
    QCOMPARE(fakeAudio->startCalls, 1);
    QVERIFY(fakeAudio->resourcesReleased());
}

void AppControllerTest::stopDoesNotRestartLoopingPlayback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));

    controller.setLoopEnabled(true);
    controller.play();
    QCOMPARE(fakeAudio->startCalls, 1);
    QVERIFY(fakeAudio->playing());

    controller.stop();
    QVERIFY(!fakeAudio->playing());
    fakeAudio->finishCurrentBuffer();
    QVERIFY(!fakeAudio->playing());
    QCOMPARE(fakeAudio->startCalls, 1);
    QVERIFY(fakeAudio->resourcesReleased());
}

void AppControllerTest::playOnceStopsAtNaturalCompletion()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));

    controller.play();
    QVERIFY(fakeAudio->playing());
    QVERIFY(!fakeAudio->loopEnabled());

    fakeAudio->finishCurrentBuffer();
    QVERIFY(!fakeAudio->playing());
    QCOMPARE(fakeAudio->startCalls, 1);
    QVERIFY(fakeAudio->resourcesReleased());
}

void AppControllerTest::previewInterruptsCompositionLoopPolicy()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));

    controller.setLoopEnabled(true);
    controller.play();
    QVERIFY(fakeAudio->loopEnabled());
    QVERIFY(controller.compositionPlaying());

    controller.selectPitched(2);
    QVERIFY(fakeAudio->playing());
    QVERIFY(!controller.compositionPlaying());
    QVERIFY(!fakeAudio->loopEnabled());
    QCOMPARE(fakeAudio->startCalls, 2);

    controller.setLoopEnabled(false);
    controller.setLoopEnabled(true);
    QVERIFY(!fakeAudio->loopEnabled());
    fakeAudio->finishCurrentBuffer();
    QVERIFY(!fakeAudio->playing());
    QCOMPARE(fakeAudio->startCalls, 2);
    QVERIFY(fakeAudio->resourcesReleased());
}

void AppControllerTest::previewActivityIsSeparateFromCompositionPlayback()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));
    QSignalSpy compositionPlayingChanged(
        &controller, &AppController::compositionPlayingChanged);

    controller.selectPitched(2);
    QVERIFY(controller.playing());
    QVERIFY(!controller.compositionPlaying());
    QCOMPARE(controller.playbackStep(), -1);
    QCOMPARE(compositionPlayingChanged.count(), 0);

    controller.play();
    QVERIFY(controller.playing());
    QVERIFY(controller.compositionPlaying());
    QCOMPARE(controller.playbackStep(), 0);
    QCOMPARE(fakeAudio->startCalls, 2);
    QCOMPARE(compositionPlayingChanged.count(), 1);

    fakeAudio->finishCurrentBuffer();
    QVERIFY(!controller.playing());
    QVERIFY(!controller.compositionPlaying());
    QCOMPARE(controller.playbackStep(), -1);
    QCOMPARE(compositionPlayingChanged.count(), 2);
}

void AppControllerTest::undoDuringPlaybackStopsAndClearsQueuedAudio()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));

    controller.selectPitched(2);
    QVERIFY(controller.placePitched(7, 4));
    QCOMPARE(controller.composition()->rowCount(), 1);
    controller.setLoopEnabled(true);
    controller.play();
    fakeAudio->finishCurrentBuffer();
    QCOMPARE(controller.playbackCycle(), 1);
    QVERIFY(controller.playing());
    QVERIFY(!fakeAudio->resourcesReleased());

    QVERIFY(controller.undo());

    QCOMPARE(controller.composition()->rowCount(), 0);
    QVERIFY(!controller.playing());
    QVERIFY(fakeAudio->resourcesReleased());
    QCOMPARE(controller.playbackStep(), -1);
    QCOMPARE(controller.playbackCycle(), 0);
    QVERIFY(controller.playbackStatus().isEmpty());
}

void AppControllerTest::pitchedPlacementAndReplacementStopBeforeMutationThenPreview()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));
    controller.selectPitched(3);
    QVERIFY(controller.placePitched(1, 3));

    bool placementSawStoppedPlayback = false;
    connect(controller.composition(), &QAbstractItemModel::rowsAboutToBeInserted,
            this, [&] {
                placementSawStoppedPlayback = !controller.playing()
                    && !controller.compositionPlaying()
                    && fakeAudio->resourcesReleased()
                    && controller.playbackStep() == -1;
            });
    controller.play();
    QVERIFY(controller.placePitched(6, 4));
    QVERIFY(placementSawStoppedPlayback);
    QVERIFY(controller.playing());
    QVERIFY(!controller.compositionPlaying());
    QCOMPARE(controller.playbackStep(), -1);

    bool replacementSawStoppedPlayback = false;
    const QJsonObject beforeReplacement = controller.composition()->toJson();
    const QString originalId = controller.composition()->data(
        controller.composition()->index(0), CompositionModel::IdRole).toString();
    connect(controller.composition(), &QAbstractItemModel::dataChanged,
            this, [&] {
                replacementSawStoppedPlayback = !controller.playing()
                    && !controller.compositionPlaying()
                    && fakeAudio->resourcesReleased()
                    && controller.playbackStep() == -1;
            });
    controller.selectPitched(2);
    controller.play();
    QVERIFY(controller.placePitched(1, 3));
    QVERIFY(replacementSawStoppedPlayback);
    QVERIFY(controller.playing());
    QVERIFY(!controller.compositionPlaying());
    QCOMPARE(controller.playbackStep(), -1);
    QVERIFY(controller.composition()->data(
                controller.composition()->index(0), CompositionModel::IdRole).toString()
            != originalId);
    QVERIFY(controller.undo());
    QCOMPARE(controller.composition()->toJson(), beforeReplacement);
}

void AppControllerTest::percussionPlacementAndExistingTapPreview()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));
    controller.selectPercussion(1);
    QVERIFY(controller.placePercussion(1));

    bool placementSawStoppedPlayback = false;
    connect(controller.composition(), &QAbstractItemModel::rowsAboutToBeInserted,
            this, [&] {
                placementSawStoppedPlayback = !controller.playing()
                    && !controller.compositionPlaying()
                    && fakeAudio->resourcesReleased()
                    && controller.playbackStep() == -1;
            });
    controller.play();
    QVERIFY(controller.placePercussion(6));
    QVERIFY(placementSawStoppedPlayback);
    QVERIFY(controller.playing());
    QVERIFY(!controller.compositionPlaying());
    QCOMPARE(controller.playbackStep(), -1);

    QSignalSpy dataChanged(controller.composition(), &QAbstractItemModel::dataChanged);
    const int previewStarts = fakeAudio->startCalls;
    QVERIFY(controller.placePercussion(1));
    QCOMPARE(dataChanged.count(), 0);
    QCOMPARE(fakeAudio->startCalls, previewStarts + 1);
    QVERIFY(controller.playing());
    QVERIFY(!controller.compositionPlaying());
    QCOMPARE(controller.playbackStep(), -1);
}

void AppControllerTest::unchangedPitchedPlacementPreviewsWithoutSavingOrAddingUndo()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    int saveCalls = 0;
    AppController controller(
        directory.filePath("autosave.json"), true, std::move(audio),
        [&saveCalls](const QString &, const CompositionModel &) {
            ++saveCalls;
            return true;
        });
    controller.selectPitched(3);
    QVERIFY(controller.placePitched(1, 3));
    QCOMPARE(saveCalls, 1);
    const QJsonObject original = controller.composition()->toJson();
    const QString originalId = controller.composition()->data(
        controller.composition()->index(0), CompositionModel::IdRole).toString();
    QSignalSpy dataChanged(controller.composition(), &QAbstractItemModel::dataChanged);
    const int previewStarts = fakeAudio->startCalls;

    QVERIFY(controller.placePitched(1, 3));

    QCOMPARE(fakeAudio->startCalls, previewStarts + 1);
    QCOMPARE(saveCalls, 1);
    QCOMPARE(controller.composition()->toJson(), original);
    QCOMPARE(controller.composition()->data(
                 controller.composition()->index(0), CompositionModel::IdRole).toString(),
             originalId);
    QCOMPARE(dataChanged.count(), 0);
    QVERIFY(controller.undo());
    QCOMPARE(controller.composition()->rowCount(), 0);
}

void AppControllerTest::unchangedPitchedPlacementKeepsCompositionPlaybackAndIndicators()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));
    controller.selectPitched(3);
    QVERIFY(controller.placePitched(1, 3));
    controller.setLoopEnabled(true);
    controller.play();
    fakeAudio->finishCurrentBuffer();
    QCOMPARE(controller.playbackCycle(), 1);
    const int playbackStep = controller.playbackStep();
    const int startCalls = fakeAudio->startCalls;
    const int stopCalls = fakeAudio->stopCalls;

    QVERIFY(controller.placePitched(1, 3));

    QVERIFY(controller.playing());
    QVERIFY(controller.compositionPlaying());
    QCOMPARE(fakeAudio->startCalls, startCalls);
    QCOMPARE(fakeAudio->stopCalls, stopCalls);
    QCOMPARE(controller.playbackStep(), playbackStep);
    QCOMPARE(controller.playbackCycle(), 1);
}

void AppControllerTest::unchangedPercussionPlacementDoesNotRetryFailedSave()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    int saveCalls = 0;
    AppController controller(
        directory.filePath("autosave.json"), true, std::move(audio),
        [&saveCalls](const QString &, const CompositionModel &) {
            ++saveCalls;
            return false;
        });
    QSignalSpy saveFailedChanged(&controller, &AppController::saveFailedChanged);
    controller.selectPercussion(1);
    QVERIFY(!controller.placePercussion(1));
    QCOMPARE(saveCalls, 1);
    QVERIFY(controller.saveFailed());
    QCOMPARE(saveFailedChanged.count(), 1);
    const QJsonObject original = controller.composition()->toJson();
    const int previewStarts = fakeAudio->startCalls;

    QVERIFY(controller.placePercussion(1));

    QCOMPARE(fakeAudio->startCalls, previewStarts + 1);
    QCOMPARE(saveCalls, 1);
    QCOMPARE(controller.composition()->toJson(), original);
    QVERIFY(controller.saveFailed());
    QCOMPARE(saveFailedChanged.count(), 1);
    QVERIFY(!controller.undo());
    QCOMPARE(controller.composition()->rowCount(), 0);
}

void AppControllerTest::unchangedPercussionPlacementKeepsCompositionPlaybackAndIndicators()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));
    controller.selectPercussion(1);
    QVERIFY(controller.placePercussion(1));
    controller.setLoopEnabled(true);
    controller.play();
    fakeAudio->finishCurrentBuffer();
    QCOMPARE(controller.playbackCycle(), 1);
    const int playbackStep = controller.playbackStep();
    const int startCalls = fakeAudio->startCalls;
    const int stopCalls = fakeAudio->stopCalls;

    QVERIFY(controller.placePercussion(1));

    QVERIFY(controller.playing());
    QVERIFY(controller.compositionPlaying());
    QCOMPARE(fakeAudio->startCalls, startCalls);
    QCOMPARE(fakeAudio->stopCalls, stopCalls);
    QCOMPARE(controller.playbackStep(), playbackStep);
    QCOMPARE(controller.playbackCycle(), 1);
}

void AppControllerTest::emptyEraserClicksKeepCompositionPlaybackAndIndicators()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));
    controller.setLoopEnabled(true);
    controller.play();
    fakeAudio->finishCurrentBuffer();
    QCOMPARE(controller.playbackCycle(), 1);
    const int playbackStep = controller.playbackStep();
    const int stopCalls = fakeAudio->stopCalls;

    QVERIFY(!controller.eraseAt(QStringLiteral("pitched"), 0, 0));
    QVERIFY(!controller.eraseAt(QStringLiteral("percussion"), 0, 0));

    QVERIFY(controller.playing());
    QVERIFY(controller.compositionPlaying());
    QVERIFY(!fakeAudio->resourcesReleased());
    QCOMPARE(fakeAudio->stopCalls, stopCalls);
    QCOMPARE(controller.playbackStep(), playbackStep);
    QCOMPARE(controller.playbackCycle(), 1);
    QVERIFY(!controller.playbackStatus().isEmpty());
}

void AppControllerTest::eraserStopsPlaybackBeforeMutation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));
    controller.selectPitched(0);
    QVERIFY(controller.placePitched(5, 2));

    bool removalSawStoppedPlayback = false;
    connect(controller.composition(), &QAbstractItemModel::rowsAboutToBeRemoved,
            this, [&] {
                removalSawStoppedPlayback = !controller.playing()
                    && fakeAudio->resourcesReleased()
                    && controller.playbackStep() == -1;
            });
    controller.play();
    QVERIFY(controller.eraseAt(QStringLiteral("pitched"), 5, 2));

    QVERIFY(removalSawStoppedPlayback);
    QVERIFY(!controller.playing());
    QVERIFY(fakeAudio->resourcesReleased());
    QCOMPARE(controller.playbackStep(), -1);
    QCOMPARE(controller.playbackCycle(), 0);
}

void AppControllerTest::addMeasureStopsPlaybackBeforeMutation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));

    bool measureChangeSawStoppedPlayback = false;
    connect(controller.composition(), &CompositionModel::measureCountChanged,
            this, [&] {
                measureChangeSawStoppedPlayback = !controller.playing()
                    && fakeAudio->resourcesReleased()
                    && controller.playbackStep() == -1;
            });
    controller.play();
    QVERIFY(controller.addMeasure());

    QVERIFY(measureChangeSawStoppedPlayback);
    QCOMPARE(controller.composition()->measureCount(), 3);
    QVERIFY(!controller.playing());
    QVERIFY(fakeAudio->resourcesReleased());
    QCOMPARE(controller.playbackStep(), -1);
    QCOMPARE(controller.playbackCycle(), 0);
}

void AppControllerTest::removeMeasurePersistsAndUndoRestoresExactEvents()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath("autosave.json");
    QJsonObject beforeRemoval;

    {
        AppController controller(path, false);
        QVERIFY(controller.addMeasure());
        controller.selectPitched(3);
        QVERIFY(controller.placePitched(8, 6));
        controller.selectPercussion(1);
        QVERIFY(controller.placePercussion(11));
        beforeRemoval = controller.composition()->toJson();

        QVERIFY(controller.removeMeasure());
        QCOMPARE(controller.composition()->measureCount(), 2);
        QCOMPARE(controller.composition()->rowCount(), 0);
    }

    {
        AppController reopened(path, false);
        QCOMPARE(reopened.composition()->measureCount(), 2);
        QCOMPARE(reopened.composition()->rowCount(), 0);
    }

    AppController controller(path, false);
    QVERIFY(controller.addMeasure());
    controller.selectPitched(3);
    QVERIFY(controller.placePitched(8, 6));
    controller.selectPercussion(1);
    QVERIFY(controller.placePercussion(11));
    beforeRemoval = controller.composition()->toJson();
    QVERIFY(controller.removeMeasure());
    QVERIFY(controller.undo());
    QCOMPARE(controller.composition()->toJson(), beforeRemoval);

    AppController restored(path, false);
    QCOMPARE(restored.composition()->toJson(), beforeRemoval);
}

void AppControllerTest::removeMeasureStopsPlaybackBeforeMutation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));
    QVERIFY(controller.addMeasure());
    controller.setLoopEnabled(true);

    bool measureChangeSawStoppedPlayback = false;
    connect(controller.composition(), &CompositionModel::measureCountChanged,
            this, [&] {
                measureChangeSawStoppedPlayback = !controller.playing()
                    && fakeAudio->resourcesReleased()
                    && controller.playbackStep() == -1
                    && controller.playbackCycle() == 0;
            });
    controller.play();
    fakeAudio->finishCurrentBuffer();
    QCOMPARE(controller.playbackCycle(), 1);

    QVERIFY(controller.removeMeasure());

    QVERIFY(measureChangeSawStoppedPlayback);
    QCOMPARE(controller.composition()->measureCount(), 2);
    QVERIFY(!controller.playing());
    QVERIFY(fakeAudio->resourcesReleased());
    QCOMPARE(controller.playbackStep(), -1);
    QCOMPARE(controller.playbackCycle(), 0);
}

void AppControllerTest::clearSongPersistsAndUndoRestoresExactComposition()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath("autosave.json");
    AppController controller(path, false);
    QVERIFY(controller.addMeasure());
    controller.selectPitched(3);
    QVERIFY(controller.placePitched(8, 6));
    controller.selectPercussion(1);
    QVERIFY(controller.placePercussion(11));
    const QJsonObject beforeClear = controller.composition()->toJson();

    QVERIFY(controller.clearSong());
    QCOMPARE(controller.composition()->measureCount(), 2);
    QCOMPARE(controller.composition()->rowCount(), 0);

    AppController reopened(path, false);
    QCOMPARE(reopened.composition()->toJson(), CompositionModel().toJson());

    QVERIFY(controller.undo());
    QCOMPARE(controller.composition()->toJson(), beforeClear);
    AppController restored(path, false);
    QCOMPARE(restored.composition()->toJson(), beforeClear);
}

void AppControllerTest::alreadyClearSongKeepsCompositionPlaybackAndIndicators()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    int saveCalls = 0;
    AppController controller(
        directory.filePath("autosave.json"), true, std::move(audio),
        [&saveCalls](const QString &, const CompositionModel &) {
            ++saveCalls;
            return true;
        });
    controller.setLoopEnabled(true);
    controller.play();
    fakeAudio->finishCurrentBuffer();
    QCOMPARE(controller.playbackCycle(), 1);
    const int playbackStep = controller.playbackStep();
    const int stopCalls = fakeAudio->stopCalls;

    QVERIFY(!controller.clearSong());

    QCOMPARE(saveCalls, 0);
    QVERIFY(controller.playing());
    QVERIFY(controller.compositionPlaying());
    QCOMPARE(fakeAudio->stopCalls, stopCalls);
    QCOMPARE(controller.playbackStep(), playbackStep);
    QCOMPARE(controller.playbackCycle(), 1);
}

void AppControllerTest::clearSongStopsPlaybackAndPreviewBeforeMutation()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));
    controller.selectPitched(2);
    QVERIFY(controller.placePitched(7, 4));
    QVERIFY(controller.addMeasure());
    controller.setLoopEnabled(true);
    controller.play();
    fakeAudio->finishCurrentBuffer();
    QCOMPARE(controller.playbackCycle(), 1);

    bool resetBeforeMutation = false;
    connect(controller.composition(), &QAbstractItemModel::modelAboutToBeReset,
            this, [&] {
                resetBeforeMutation = !controller.playing()
                    && fakeAudio->resourcesReleased()
                    && controller.playbackStep() == -1
                    && controller.playbackCycle() == 0;
            });
    QVERIFY(controller.clearSong());
    QVERIFY(resetBeforeMutation);
    QVERIFY(controller.loopEnabled());

    QVERIFY(controller.undo());
    controller.selectPercussion(1);
    QVERIFY(controller.playing());
    QVERIFY(!fakeAudio->resourcesReleased());
    QVERIFY(controller.clearSong());
    QVERIFY(!controller.playing());
    QVERIFY(fakeAudio->resourcesReleased());
    QCOMPARE(controller.playbackStep(), -1);
    QCOMPARE(controller.playbackCycle(), 0);
}

void AppControllerTest::clearSongReportsSaveFailureWithoutLyingAboutMemoryState()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString blockedParent = directory.filePath("blocked");
    QFile blocker(blockedParent);
    QVERIFY(blocker.open(QIODevice::WriteOnly));
    QVERIFY(blocker.write("not a directory") > 0);
    blocker.close();

    AppController controller(blockedParent + QStringLiteral("/autosave.json"), false);
    QVERIFY(!controller.addMeasure());
    controller.selectPitched(1);
    QVERIFY(!controller.placePitched(8, 2));
    const QJsonObject beforeClear = controller.composition()->toJson();

    QVERIFY(!controller.clearSong());
    QCOMPARE(controller.composition()->measureCount(), 2);
    QCOMPARE(controller.composition()->rowCount(), 0);
    QVERIFY(controller.saveFailed());
    QCOMPARE(controller.saveFailureMessage(),
             QStringLiteral("Your song is here, but it is not saved yet."));

    QVERIFY(!controller.undo());
    QCOMPARE(controller.composition()->toJson(), beforeClear);
    QVERIFY(controller.saveFailed());
}

void AppControllerTest::recoversFromOutputError()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));
    QSignalSpy playingChanged(&controller, &AppController::playingChanged);
    QSignalSpy audioFailedChanged(&controller, &AppController::audioFailedChanged);

    controller.play();
    QVERIFY(controller.playing());
    fakeAudio->failWithOutputError();

    QVERIFY(!controller.playing());
    QVERIFY(controller.audioFailed());
    QVERIFY(fakeAudio->resourcesReleased());
    QCOMPARE(controller.audioFailureMessage(),
             QStringLiteral("Sound stopped. You can try Play again."));
    QCOMPARE(playingChanged.count(), 2);
    QCOMPARE(audioFailedChanged.count(), 1);

    controller.play();
    QVERIFY(controller.playing());
    QVERIFY(!controller.audioFailed());
    QVERIFY(controller.audioFailureMessage().isEmpty());
    QCOMPARE(audioFailedChanged.count(), 2);
}

void AppControllerTest::idleOutputErrorStopsInsteadOfRestartingLoop()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));

    controller.setLoopEnabled(true);
    controller.play();
    QCOMPARE(fakeAudio->startCalls, 1);

    fakeAudio->failWhileIdle();

    QVERIFY(!controller.playing());
    QVERIFY(controller.audioFailed());
    QVERIFY(fakeAudio->resourcesReleased());
    QCOMPARE(fakeAudio->startCalls, 1);

    controller.play();
    QVERIFY(controller.playing());
    QVERIFY(!controller.audioFailed());
}

void AppControllerTest::failedLoopRestartBecomesRecoverableError()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));

    controller.setLoopEnabled(true);
    controller.play();
    fakeAudio->startSucceeds = false;
    fakeAudio->finishCurrentBuffer();

    QVERIFY(!controller.playing());
    QVERIFY(controller.audioFailed());
    QCOMPARE(fakeAudio->startCalls, 2);
    QVERIFY(fakeAudio->resourcesReleased());
}

void AppControllerTest::repeatedPlayStopCyclesStayConsistent()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));
    QSignalSpy playingChanged(&controller, &AppController::playingChanged);

    for (int cycle = 0; cycle < 10; ++cycle) {
        controller.play();
        QVERIFY(controller.playing());
        controller.stop();
        fakeAudio->notifyStoppedState();
        QVERIFY(!controller.playing());
        QVERIFY(!controller.audioFailed());
    }

    QCOMPARE(fakeAudio->startCalls, 10);
    QVERIFY(fakeAudio->resourcesReleased());
    QCOMPARE(playingChanged.count(), 20);
}

void AppControllerTest::exposesPlaybackStepAndSemanticStatus()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    auto clock = std::make_unique<ManualPlaybackClock>();
    ManualPlaybackClock *manualClock = clock.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio), std::move(clock), nullptr);

    QCOMPARE(controller.playbackStep(), -1);
    QVERIFY(controller.playbackStatus().isEmpty());

    controller.play();
    QCOMPARE(controller.playbackStep(), 0);
    QCOMPARE(controller.playbackCycle(), 0);
    QCOMPARE(controller.playbackStatus(), QStringLiteral("Playing beat 1 of 8."));

    manualClock->setElapsed(60000 / 112);
    QVERIFY(QMetaObject::invokeMethod(
        &controller, "refreshPlaybackProgress", Qt::DirectConnection));
    QCOMPARE(controller.playbackStep(), 1);
    QCOMPARE(controller.playbackStatus(), QStringLiteral("Playing beat 2 of 8."));

    controller.stop();
    QCOMPARE(controller.playbackStep(), -1);
    QVERIFY(controller.playbackStatus().isEmpty());
}

void AppControllerTest::reportsLoopRestart()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto audio = std::make_unique<FakeAudioEngine>();
    FakeAudioEngine *fakeAudio = audio.get();
    AppController controller(directory.filePath("autosave.json"), true,
                             std::move(audio));
    controller.setLoopEnabled(true);
    QSignalSpy restarted(&controller, &AppController::loopRestarted);

    controller.play();
    fakeAudio->finishCurrentBuffer();

    QCOMPARE(controller.playbackStep(), 0);
    QCOMPARE(controller.playbackCycle(), 1);
    QCOMPARE(controller.playbackStatus(), QStringLiteral("Loop 2, beat 1 of 8."));
    QCOMPARE(restarted.count(), 1);
}

QTEST_MAIN(AppControllerTest)
#include "tst_appcontroller.moc"
