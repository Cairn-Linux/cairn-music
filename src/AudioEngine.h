#pragma once

#include <QAudioSink>
#include <QBuffer>
#include <QObject>

#include <memory>

class AudioEngine : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(bool error READ hasError NOTIFY errorChanged)

public:
    explicit AudioEngine(QObject *parent = nullptr);
    explicit AudioEngine(bool initializeOutput, QObject *parent = nullptr);
    ~AudioEngine() override = default;

    [[nodiscard]] virtual bool playing() const noexcept;
    [[nodiscard]] virtual bool loopEnabled() const noexcept;
    [[nodiscard]] bool hasError() const noexcept;
    virtual bool play(const QByteArray &pcm, bool loop = false);
    virtual void setLoopEnabled(bool enabled);
    Q_INVOKABLE virtual void stop();

signals:
    void playingChanged();
    void loopRestarted();
    void errorChanged();

protected:
    void handleState(QAudio::State state);
    virtual bool restartPlayback();
    [[nodiscard]] virtual bool outputAvailable() const noexcept;
    [[nodiscard]] virtual QAudio::Error outputError() const noexcept;
    [[nodiscard]] virtual QAudio::State outputState() const noexcept;
    virtual void startOutput(QIODevice *device);
    virtual void stopOutput();
    void setError(bool error);
    [[nodiscard]] bool activeBufferOpen() const noexcept;
    [[nodiscard]] qsizetype bufferedByteCount() const noexcept;

private:
    void releasePlayback();

    QByteArray m_pcm;
    QBuffer m_buffer;
    std::unique_ptr<QAudioSink> m_sink;
    bool m_loop = false;
    bool m_playing = false;
    bool m_error = false;
};
