#include "AudioRenderer.h"

#include <QtEndian>
#include <QJsonArray>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {
constexpr double pi = 3.14159265358979323846;
// Composition playback reserves enough deterministic headroom for the legal
// three pitched plus two percussion voices. Preview rendering stays unchanged.
constexpr qint64 compositionHeadroomDivisor = 4;

void writeStereo(QByteArray &pcm, int frame, double value)
{
    const auto sample = static_cast<qint16>(std::clamp(value, -1.0, 1.0) * 24000.0);
    const qint16 little = qToLittleEndian(sample);
    const int offset = frame * int(sizeof(qint16) * 2);
    std::memcpy(pcm.data() + offset, &little, sizeof(little));
    std::memcpy(pcm.data() + offset + sizeof(little), &little, sizeof(little));
}

double pitchFrequency(int row)
{
    static constexpr double naturals[] = {261.6256, 293.6648, 329.6276, 349.2282,
                                          391.9954, 440.0, 493.8833};
    return naturals[std::clamp(row, 0, 6)];
}
}

QByteArray AudioRenderer::renderPitched(int soundId, int pitchRow, int durationMs)
{
    if (soundId < 0 || soundId >= 4 || pitchRow < 0 || pitchRow >= 7
        || durationMs <= 0 || durationMs > 4000) {
        return {};
    }

    const int frames = sampleRate * durationMs / 1000;
    QByteArray pcm(frames * int(sizeof(qint16) * 2), Qt::Uninitialized);
    const double frequency = pitchFrequency(pitchRow);
    double bubblePhase = 0.0;

    for (int frame = 0; frame < frames; ++frame) {
        const double time = double(frame) / sampleRate;
        const double progress = double(frame) / std::max(1, frames - 1);
        const double attack = std::min(1.0, progress * 40.0);
        const double release = std::min(1.0, (1.0 - progress) * 12.0);
        double value = 0.0;
        switch (soundId) {
        case 0: // warm round tone
            value = std::sin(2.0 * pi * frequency * time) * attack * release;
            break;
        case 1: { // pluck
            const double phase = std::fmod(frequency * time, 1.0);
            value = (4.0 * std::abs(phase - 0.5) - 1.0)
                * std::exp(-6.0 * progress) * attack;
            break;
        }
        case 2: // bell
            value = (std::sin(2.0 * pi * frequency * time)
                     + 0.45 * std::sin(2.0 * pi * frequency * 2.71 * time))
                * std::exp(-4.5 * progress) * attack * 0.7;
            break;
        case 3: { // playful wobble
            const double wobble = 1.0 + 0.025 * std::sin(2.0 * pi * 7.0 * time);
            bubblePhase += 2.0 * pi * frequency * wobble / sampleRate;
            value = (std::sin(bubblePhase)
                     + 0.22 * std::sin(2.0 * pi * frequency * 2.0 * time))
                * attack * release * 0.8;
            break;
        }
        }
        writeStereo(pcm, frame, value);
    }
    return pcm;
}

QByteArray AudioRenderer::renderPercussion(int soundId, int durationMs)
{
    if (soundId < 0 || soundId >= 2 || durationMs <= 0 || durationMs > 2000) {
        return {};
    }

    const int frames = sampleRate * durationMs / 1000;
    QByteArray pcm(frames * int(sizeof(qint16) * 2), Qt::Uninitialized);
    std::uint32_t noise = 0xC41A5EEDu;

    for (int frame = 0; frame < frames; ++frame) {
        const double time = double(frame) / sampleRate;
        const double progress = double(frame) / std::max(1, frames - 1);
        double value = 0.0;
        if (soundId == 0) {
            const double frequency = 95.0 - 50.0 * progress;
            value = std::sin(2.0 * pi * frequency * time) * std::exp(-8.0 * progress);
        } else {
            noise = noise * 1664525u + 1013904223u;
            const double white = (double((noise >> 8) & 0xFFFFu) / 32767.5) - 1.0;
            const double burst = std::fmod(time, 0.055) < 0.018 ? 1.0 : 0.25;
            value = white * burst * std::exp(-7.0 * progress) * 0.75;
        }
        writeStereo(pcm, frame, value);
    }
    return pcm;
}

QByteArray AudioRenderer::renderComposition(const QJsonObject &project, int tempoBpm)
{
    const int measures = std::clamp(project.value(QStringLiteral("measures")).toInt(2), 2, 8);
    const int tempo = std::clamp(tempoBpm, 40, 240);
    const int beatMs = 60000 / tempo;
    const int bytesPerFrame = int(sizeof(qint16) * 2);
    const qsizetype totalFrames = qsizetype(sampleRate) * measures * 4 * beatMs / 1000;
    const qsizetype totalSamples = totalFrames * 2;
    QVector<qint64> mix(totalSamples, 0);

    const QJsonArray tokens = project.value(QStringLiteral("tokens")).toArray();
    for (const QJsonValue &value : tokens) {
        const QJsonObject token = value.toObject();
        const int step = token.value(QStringLiteral("step")).toInt(-1);
        if (step < 0 || step >= measures * 4) {
            continue;
        }
        const QString kind = token.value(QStringLiteral("kind")).toString();
        const int sound = token.value(QStringLiteral("sound")).toInt(-1);
        const int row = token.value(QStringLiteral("row")).toInt(-1);
        const QByteArray voice = kind == QStringLiteral("percussion")
            ? renderPercussion(sound, std::min(beatMs, 240))
            : renderPitched(sound, row, std::min(beatMs, 420));
        const qsizetype startSample = qsizetype(sampleRate) * step * beatMs / 1000 * 2;
        for (qsizetype offset = 0; offset + qsizetype(sizeof(qint16)) <= voice.size()
             && startSample + offset / qsizetype(sizeof(qint16)) < mix.size();
             offset += qsizetype(sizeof(qint16))) {
            const qint16 source = qFromLittleEndian<qint16>(voice.constData() + offset);
            mix[startSample + offset / qsizetype(sizeof(qint16))] += source;
        }
    }

    QByteArray result(totalFrames * bytesPerFrame, Qt::Uninitialized);
    for (qsizetype sampleIndex = 0; sampleIndex < mix.size(); ++sampleIndex) {
        const qint64 scaled = mix.at(sampleIndex) / compositionHeadroomDivisor;
        const qint16 sample = static_cast<qint16>(std::clamp(
            scaled, qint64(std::numeric_limits<qint16>::min()),
            qint64(std::numeric_limits<qint16>::max())));
        qToLittleEndian(sample,
                        result.data() + sampleIndex * int(sizeof(qint16)));
    }
    return result;
}
