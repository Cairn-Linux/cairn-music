#include <QtTest>

#include "AudioRenderer.h"

#include <QtEndian>

#include <algorithm>
#include <limits>

namespace {
QList<qint16> samples(const QByteArray &pcm)
{
    QList<qint16> decoded;
    decoded.reserve(pcm.size() / qsizetype(sizeof(qint16)));
    for (qsizetype offset = 0; offset + qsizetype(sizeof(qint16)) <= pcm.size();
         offset += qsizetype(sizeof(qint16))) {
        decoded.append(qFromLittleEndian<qint16>(pcm.constData() + offset));
    }
    return decoded;
}

qint32 peakMagnitude(const QByteArray &pcm)
{
    qint32 peak = 0;
    for (const qint16 sample : samples(pcm)) {
        peak = std::max(peak, std::abs(qint32(sample)));
    }
    return peak;
}

qsizetype hardClippedSampleCount(const QByteArray &pcm)
{
    const QList<qint16> decoded = samples(pcm);
    return std::count_if(decoded.cbegin(), decoded.cend(), [](qint16 sample) {
        return sample == std::numeric_limits<qint16>::min()
            || sample == std::numeric_limits<qint16>::max();
    });
}

QJsonObject projectWithVoices(const QList<QJsonObject> &voices)
{
    QJsonArray tokens;
    for (const QJsonObject &voice : voices) {
        tokens.append(voice);
    }
    return QJsonObject{{"version", 1}, {"measures", 2}, {"tokens", tokens}};
}
}

class AudioRendererTest : public QObject
{
    Q_OBJECT

private slots:
    void rendersFourDistinctPitchedSounds();
    void rendersTwoDistinctPercussionSounds();
    void rendersCompositionAtTheStoredStep();
    void singlePreviewsRetainBaselinePeaks();
    void singleVoiceCompositionAppliesHeadroomAfterAccumulation();
    void stressChordUsesHeadroomWithoutHardClipping();
    void fullCapOverlapUsesHeadroomWithoutHardClipping();
    void repeatedCompositionRenderProducesIdenticalPcm();
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

void AudioRendererTest::singlePreviewsRetainBaselinePeaks()
{
    const QByteArray pitched = AudioRenderer::renderPitched(0, 3, 420);
    const QByteArray percussion = AudioRenderer::renderPercussion(1, 240);

    QVERIFY(!pitched.isEmpty());
    QVERIFY(!percussion.isEmpty());
    QCOMPARE(peakMagnitude(pitched), qint32(23999));
    QCOMPARE(peakMagnitude(percussion), qint32(17478));
}

void AudioRendererTest::singleVoiceCompositionAppliesHeadroomAfterAccumulation()
{
    const QJsonObject voice{{"id", "one"}, {"kind", "pitched"}, {"step", 0},
                            {"row", 3}, {"sound", 0}};
    const QByteArray preview = AudioRenderer::renderPitched(0, 3, 420);
    const QByteArray composition = AudioRenderer::renderComposition(
        projectWithVoices({voice}), 112);

    QVERIFY(!preview.isEmpty());
    QVERIFY(composition.size() >= preview.size());
    for (qsizetype offset = 0; offset < preview.size();
         offset += qsizetype(sizeof(qint16))) {
        const qint16 previewSample = qFromLittleEndian<qint16>(preview.constData() + offset);
        const qint16 compositionSample = qFromLittleEndian<qint16>(
            composition.constData() + offset);
        QCOMPARE(compositionSample, qint16(previewSample / 4));
    }
}

void AudioRendererTest::stressChordUsesHeadroomWithoutHardClipping()
{
    const QList<QJsonObject> voices{
        QJsonObject{{"id", "low"}, {"kind", "pitched"}, {"step", 0},
                    {"row", 0}, {"sound", 0}},
        QJsonObject{{"id", "mid"}, {"kind", "pitched"}, {"step", 0},
                    {"row", 2}, {"sound", 0}},
        QJsonObject{{"id", "high"}, {"kind", "pitched"}, {"step", 0},
                    {"row", 4}, {"sound", 0}},
    };

    const QByteArray pcm = AudioRenderer::renderComposition(projectWithVoices(voices), 112);
    QVERIFY(!pcm.isEmpty());
    QCOMPARE(hardClippedSampleCount(pcm), qsizetype(0));
    QVERIFY(peakMagnitude(pcm) < std::numeric_limits<qint16>::max());
}

void AudioRendererTest::fullCapOverlapUsesHeadroomWithoutHardClipping()
{
    const QList<QJsonObject> voices{
        QJsonObject{{"id", "pitch-low"}, {"kind", "pitched"}, {"step", 0},
                    {"row", 0}, {"sound", 0}},
        QJsonObject{{"id", "pitch-mid"}, {"kind", "pitched"}, {"step", 0},
                    {"row", 2}, {"sound", 0}},
        QJsonObject{{"id", "pitch-high"}, {"kind", "pitched"}, {"step", 0},
                    {"row", 4}, {"sound", 0}},
        QJsonObject{{"id", "drum-thump"}, {"kind", "percussion"}, {"step", 0},
                    {"row", -1}, {"sound", 0}},
        QJsonObject{{"id", "drum-clap"}, {"kind", "percussion"}, {"step", 0},
                    {"row", -1}, {"sound", 1}},
    };

    const QByteArray pcm = AudioRenderer::renderComposition(projectWithVoices(voices), 112);
    QVERIFY(!pcm.isEmpty());
    QCOMPARE(hardClippedSampleCount(pcm), qsizetype(0));
    QVERIFY(peakMagnitude(pcm) < std::numeric_limits<qint16>::max());
}

void AudioRendererTest::repeatedCompositionRenderProducesIdenticalPcm()
{
    const QList<QJsonObject> voices{
        QJsonObject{{"id", "pitch"}, {"kind", "pitched"}, {"step", 0},
                    {"row", 4}, {"sound", 2}},
        QJsonObject{{"id", "drum"}, {"kind", "percussion"}, {"step", 0},
                    {"row", -1}, {"sound", 1}},
    };
    const QJsonObject project = projectWithVoices(voices);

    const QByteArray first = AudioRenderer::renderComposition(project, 112);
    const QByteArray second = AudioRenderer::renderComposition(project, 112);
    QVERIFY(!first.isEmpty());
    QCOMPARE(first, second);
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
