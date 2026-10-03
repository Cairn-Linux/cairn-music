#include <QtTest>

#include "AudioRenderer.h"

class AudioRendererTest : public QObject
{
    Q_OBJECT

private slots:
    void rendersFourDistinctPitchedSounds();
    void rendersTwoDistinctPercussionSounds();
    void rendersCompositionAtTheStoredStep();
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

QTEST_MAIN(AudioRendererTest)
#include "tst_audiorenderer.moc"
