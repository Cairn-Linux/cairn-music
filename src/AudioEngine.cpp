#include "AudioEngine.h"

#include "AudioRenderer.h"

#include <QAudioDevice>
#include <QMediaDevices>

AudioEngine::AudioEngine(QObject *parent)
    : AudioEngine(true, parent)
{
}

AudioEngine::AudioEngine(bool initializeOutput, QObject *parent)
    : QObject(parent)
{
    if (!initializeOutput) {
        return;
    }

    QAudioFormat format;
    format.setSampleRate(AudioRenderer::sampleRate);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);

    const QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (!device.isNull() && device.isFormatSupported(format)) {
        m_sink = std::make_unique<QAudioSink>(device, format, this);
        connect(m_sink.get(), &QAudioSink::stateChanged,
                this, &AudioEngine::handleState);
    } else {
        m_error = true;
    }
}

bool AudioEngine::playing() const noexcept
{
    return m_playing;
}

bool AudioEngine::loopEnabled() const noexcept
{
    return m_loop;
}

bool AudioEngine::hasError() const noexcept
{
    return m_error;
}

void AudioEngine::setError(bool error)
{
    if (m_error == error) {
        return;
    }
    m_error = error;
    emit errorChanged();
}

void AudioEngine::setLoopEnabled(bool enabled)
{
    m_loop = enabled;
}

bool AudioEngine::play(const QByteArray &pcm, bool loop)
{
    if (!outputAvailable() || pcm.isEmpty()) {
        setError(true);
        return false;
    }

    stopOutput();
    m_buffer.close();
    m_pcm = pcm;
    m_loop = loop;
    m_buffer.setData(m_pcm);
    if (!m_buffer.open(QIODevice::ReadOnly)) {
        setError(true);
        return false;
    }
    setError(false);
    startOutput(&m_buffer);
    if (outputError() != QAudio::NoError || outputState() == QAudio::StoppedState) {
        releasePlayback();
        setError(true);
        return false;
    }
    if (!m_playing) {
        m_playing = true;
        emit playingChanged();
    }
    return true;
}

void AudioEngine::stop()
{
    if (outputAvailable()) {
        stopOutput();
    }
    releasePlayback();
}

void AudioEngine::releasePlayback()
{
    m_buffer.close();
    m_buffer.setData({});
    m_pcm.clear();
    m_loop = false;
    if (m_playing) {
        m_playing = false;
        emit playingChanged();
    }
}

void AudioEngine::handleState(QAudio::State state)
{
    // Handle output errors before interpreting Idle as either natural
    // completion or a loop boundary.
    if (outputError() != QAudio::NoError) {
        stop();
        setError(true);
        return;
    }
    if (state != QAudio::IdleState) {
        return;
    }
    if (m_loop) {
        if (restartPlayback()) {
            emit loopRestarted();
            return;
        }
        stop();
        setError(true);
    } else {
        stop();
    }
}

bool AudioEngine::restartPlayback()
{
    if (!outputAvailable() || !m_buffer.isOpen() || !m_buffer.seek(0)) {
        return false;
    }
    startOutput(&m_buffer);
    return outputError() == QAudio::NoError && outputState() != QAudio::StoppedState;
}

bool AudioEngine::outputAvailable() const noexcept
{
    return m_sink != nullptr;
}

QAudio::Error AudioEngine::outputError() const noexcept
{
    return m_sink ? m_sink->error() : QAudio::OpenError;
}

QAudio::State AudioEngine::outputState() const noexcept
{
    return m_sink ? m_sink->state() : QAudio::StoppedState;
}

void AudioEngine::startOutput(QIODevice *device)
{
    m_sink->start(device);
}

void AudioEngine::stopOutput()
{
    m_sink->stop();
}

bool AudioEngine::activeBufferOpen() const noexcept
{
    return m_buffer.isOpen();
}

qsizetype AudioEngine::bufferedByteCount() const noexcept
{
    return m_pcm.size() + m_buffer.data().size();
}
