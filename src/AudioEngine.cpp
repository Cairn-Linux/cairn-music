#include "AudioEngine.h"

#include "AudioRenderer.h"

#include <QAudioDevice>
#include <QMediaDevices>

AudioEngine::AudioEngine(QObject *parent)
    : QObject(parent)
{
    QAudioFormat format;
    format.setSampleRate(AudioRenderer::sampleRate);
    format.setChannelCount(2);
    format.setSampleFormat(QAudioFormat::Int16);

    const QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (!device.isNull() && device.isFormatSupported(format)) {
        m_sink = std::make_unique<QAudioSink>(device, format, this);
        connect(m_sink.get(), &QAudioSink::stateChanged,
                this, &AudioEngine::handleState);
    }
}

bool AudioEngine::playing() const noexcept
{
    return m_playing;
}

bool AudioEngine::play(const QByteArray &pcm, bool loop)
{
    if (!m_sink || pcm.isEmpty()) {
        return false;
    }

    m_sink->stop();
    m_buffer.close();
    m_pcm = pcm;
    m_loop = loop;
    m_buffer.setData(m_pcm);
    if (!m_buffer.open(QIODevice::ReadOnly)) {
        return false;
    }
    m_sink->start(&m_buffer);
    if (!m_playing) {
        m_playing = true;
        emit playingChanged();
    }
    return true;
}

void AudioEngine::stop()
{
    if (m_sink) {
        m_sink->stop();
    }
    m_buffer.close();
    if (m_playing) {
        m_playing = false;
        emit playingChanged();
    }
}

void AudioEngine::handleState(QAudio::State state)
{
    if (state != QAudio::IdleState) {
        return;
    }
    if (m_loop && m_sink && m_buffer.isOpen()) {
        m_buffer.seek(0);
        m_sink->start(&m_buffer);
        return;
    }
    stop();
}
