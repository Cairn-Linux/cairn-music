#include <QtTest>

#include "AudioRenderer.h"

class AudioRendererTest : public QObject
{
    Q_OBJECT

private slots:
    void rendersFourDistinctPitchedSounds();
    void rendersTwoDistinctPercussionSounds();
    void rendersCompositionAtTheStoredStep();
    void equivalentVoiceOrderProducesIdenticalPcm();
    void rendersMaximumLengthFinalStepSafely();
};

void AudioRendererTest::rendersFourDistinctPitchedSounds()
{
    QList<QByteArray> sounds;
    for (int soundId = 0; soundId < 4; ++soundId) {
        const QByteArray pcm = AudioRenderer::renderPitched(soundId, 3, 180);
        QVERIFY(!pcm.isEmpty());
        QCOMPARE(pcm.size() % int(sizeof(qint16) * 2), 0);
        sounds.append(pcm);
    }

    for (int left = 0; left < sounds.size(); ++left) {
        for (int right = left + 1; right < sounds.size(); ++right) {
            QVERIFY(sounds.at(left) != sounds.at(right));
        }
    }
}

void AudioRendererTest::rendersTwoDistinctPercussionSounds()
{
    const QByteArray thump = AudioRenderer::renderPercussion(0, 180);
    const QByteArray clap = AudioRenderer::renderPercussion(1, 180);

    QVERIFY(!thump.isEmpty());
    QCOMPARE(thump.size(), clap.size());
    QVERIFY(thump != clap);
}

void AudioRendererTest::rendersCompositionAtTheStoredStep()
{
    const QJsonObject project{
        {"version", 1},
        {"measures", 2},
        {"tokens", QJsonArray{QJsonObject{{"id", "one"},
                                          {"kind", "pitched"},
                                          {"step", 1},
                                          {"row", 3},
                                          {"sound", 0}}}},
    };

    const QByteArray pcm = AudioRenderer::renderComposition(project, 120);
    const int bytesPerFrame = int(sizeof(qint16) * 2);
    const int firstBeatBytes = AudioRenderer::sampleRate / 2 * bytesPerFrame;
    QVERIFY(pcm.size() > firstBeatBytes);
    QVERIFY(std::all_of(pcm.cbegin(), pcm.cbegin() + firstBeatBytes,
                        [](char value) { return value == 0; }));
    QVERIFY(std::any_of(pcm.cbegin() + firstBeatBytes, pcm.cend(),
                        [](char value) { return value != 0; }));
}

void AudioRendererTest::equivalentVoiceOrderProducesIdenticalPcm()
{
    const QList<QJsonObject> voices{
        QJsonObject{{"id", "pitch-low"}, {"kind", "pitched"}, {"step", 0},
                    {"row", 0}, {"sound", 0}},
        QJsonObject{{"id", "pitch-mid"}, {"kind", "pitched"}, {"step", 0},
                    {"row", 3}, {"sound", 1}},
        QJsonObject{{"id", "pitch-high"}, {"kind", "pitched"}, {"step", 0},
                    {"row", 6}, {"sound", 2}},
        QJsonObject{{"id", "drum-thump"}, {"kind", "percussion"}, {"step", 0},
                    {"row", -1}, {"sound", 0}},
        QJsonObject{{"id", "drum-clap"}, {"kind", "percussion"}, {"step", 0},
                    {"row", -1}, {"sound", 1}},
    };
    QJsonArray forward;
    QJsonArray reversed;
    for (const QJsonObject &voice : voices) {
        forward.append(voice);
        reversed.prepend(voice);
    }

    const QJsonObject forwardProject{{"version", 1}, {"measures", 2},
                                     {"tokens", forward}};
    const QJsonObject reversedProject{{"version", 1}, {"measures", 2},
                                      {"tokens", reversed}};

    const QByteArray forwardPcm = AudioRenderer::renderComposition(forwardProject, 120);
    const QByteArray reversedPcm = AudioRenderer::renderComposition(reversedProject, 120);
    QVERIFY(!forwardPcm.isEmpty());
    QCOMPARE(forwardPcm, reversedPcm);
}

void AudioRendererTest::rendersMaximumLengthFinalStepSafely()
{
    const QJsonObject project{
        {"version", 1},
        {"measures", 8},
        {"tokens", QJsonArray{QJsonObject{{"id", "last"},
                                          {"kind", "pitched"},
                                          {"step", 31},
                                          {"row", 3},
                                          {"sound", 0}}}},
    };

    const QByteArray pcm = AudioRenderer::renderComposition(project, 40);
    const qsizetype beatFrames = qsizetype(AudioRenderer::sampleRate) * 1500 / 1000;
    const qsizetype expectedFrames = beatFrames * 8 * 4;
    QCOMPARE(pcm.size(), expectedFrames * qsizetype(sizeof(qint16) * 2));

    const qsizetype finalStepByte = beatFrames * 31 * qsizetype(sizeof(qint16) * 2);
    QVERIFY(std::all_of(pcm.cbegin(), pcm.cbegin() + finalStepByte,
                        [](char value) { return value == 0; }));
    QVERIFY(std::any_of(pcm.cbegin() + finalStepByte, pcm.cend(),
                        [](char value) { return value != 0; }));
}

QTEST_MAIN(AudioRendererTest)
#include "tst_audiorenderer.moc"
