#pragma once

#include <QAudioSink>
#include <QBuffer>
#include <QObject>

#include <memory>

class AudioEngine final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)

public:
    explicit AudioEngine(QObject *parent = nullptr);

    [[nodiscard]] bool playing() const noexcept;
    bool play(const QByteArray &pcm, bool loop = false);
    Q_INVOKABLE void stop();

signals:
    void playingChanged();

private:
    void handleState(QAudio::State state);

    QByteArray m_pcm;
    QBuffer m_buffer;
    std::unique_ptr<QAudioSink> m_sink;
    bool m_loop = false;
    bool m_playing = false;
};
