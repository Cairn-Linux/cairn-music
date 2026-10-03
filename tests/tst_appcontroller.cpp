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

    void failWithUnderrun()
    {
        m_outputError = QAudio::UnderrunError;
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

class AppControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void placesSelectedSoundAndReopensAutosave();
    void preservesMalformedAutosaveUntilNewSongIsConfirmed();
    void reportsAutosaveFailureWhileKeepingAcceptedEditsInMemory();
    void turnsLoopOnDuringPlayback();
    void turnsLoopOffDuringPlayback();
    void stopDoesNotRestartLoopingPlayback();
    void playOnceStopsAtNaturalCompletion();
    void previewInterruptsCompositionLoopPolicy();
    void recoversFromOutputError();
    void idleUnderrunStopsInsteadOfRestartingLoop();
    void failedLoopRestartBecomesRecoverableError();
    void repeatedPlayStopCyclesStayConsistent();
};

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

    controller.selectPitched(2);
    QVERIFY(fakeAudio->playing());
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

void AppControllerTest::idleUnderrunStopsInsteadOfRestartingLoop()
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

    fakeAudio->failWithUnderrun();

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

QTEST_MAIN(AppControllerTest)
#include "tst_appcontroller.moc"
