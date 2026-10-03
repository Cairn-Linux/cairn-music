#pragma once

#include <QByteArray>
#include <QJsonObject>

class AudioRenderer
{
public:
    static constexpr int sampleRate = 48000;

    static QByteArray renderPitched(int soundId, int pitchRow, int durationMs);
    static QByteArray renderPercussion(int soundId, int durationMs);
    static QByteArray renderComposition(const QJsonObject &project, int tempoBpm);
};
