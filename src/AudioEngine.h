#pragma once

#include <QAudioSink>
#include <QBuffer>
#include <QObject>

#include <memory>

class AudioEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)

public:
    explicit AudioEngine(QObject *parent = nullptr);
    explicit AudioEngine(bool initializeOutput, QObject *parent = nullptr);
    ~AudioEngine() override = default;

    [[nodiscard]] virtual bool playing() const noexcept;
    [[nodiscard]] virtual bool loopEnabled() const noexcept;
    virtual bool play(const QByteArray &pcm, bool loop = false);
    virtual void setLoopEnabled(bool enabled);
    Q_INVOKABLE virtual void stop();

signals:
    void playingChanged();

protected:
    void handleState(QAudio::State state);
    virtual bool restartPlayback();

private:
    QByteArray m_pcm;
    QBuffer m_buffer;
    std::unique_ptr<QAudioSink> m_sink;
    bool m_loop = false;
    bool m_playing = false;
};
