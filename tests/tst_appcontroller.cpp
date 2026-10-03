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

    [[nodiscard]] bool playing() const noexcept override { return m_fakePlaying; }
    bool play(const QByteArray &pcm, bool loop) override
    {
        if (pcm.isEmpty()) {
            return false;
        }
        ++playCalls;
        AudioEngine::setLoopEnabled(loop);
        if (!m_fakePlaying) {
            m_fakePlaying = true;
            emit playingChanged();
        }
        return true;
    }

    void stop() override
    {
        if (m_fakePlaying) {
            m_fakePlaying = false;
            emit playingChanged();
        }
    }

    void finishCurrentBuffer() { handleState(QAudio::IdleState); }

    int playCalls = 0;
    int restartCalls = 0;

protected:
    bool restartPlayback() override
    {
        if (!m_fakePlaying) {
            return false;
        }
        ++restartCalls;
        return true;
    }

private:
    bool m_fakePlaying = false;
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
    QCOMPARE(fakeAudio->restartCalls, 1);
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
    QCOMPARE(fakeAudio->restartCalls, 0);
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
    QCOMPARE(fakeAudio->playCalls, 1);
    QVERIFY(fakeAudio->playing());

    controller.stop();
    QVERIFY(!fakeAudio->playing());
    fakeAudio->finishCurrentBuffer();
    QVERIFY(!fakeAudio->playing());
    QCOMPARE(fakeAudio->playCalls, 1);
    QCOMPARE(fakeAudio->restartCalls, 0);
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
    QCOMPARE(fakeAudio->restartCalls, 0);
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
    QCOMPARE(fakeAudio->playCalls, 2);

    controller.setLoopEnabled(false);
    controller.setLoopEnabled(true);
    QVERIFY(!fakeAudio->loopEnabled());
    fakeAudio->finishCurrentBuffer();
    QVERIFY(!fakeAudio->playing());
    QCOMPARE(fakeAudio->restartCalls, 0);
}

QTEST_MAIN(AppControllerTest)
#include "tst_appcontroller.moc"
