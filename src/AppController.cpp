#include "AppController.h"

#include "AudioRenderer.h"
#include "ProjectStore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUuid>

namespace {
QByteArray fileSha256(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(64 * 1024);
        if (chunk.isEmpty() && file.error() != QFile::NoError) {
            return {};
        }
        hash.addData(chunk);
    }
    return hash.result();
}
}

AppController::AppController(const QString &autosavePath, bool audioEnabled, QObject *parent)
    : AppController(autosavePath, audioEnabled, std::make_unique<AudioEngine>(), parent)
{
}

AppController::AppController(const QString &autosavePath, bool audioEnabled,
                             std::unique_ptr<AudioEngine> audio, QObject *parent)
    : QObject(parent)
    , m_audio(audio ? std::move(audio) : std::make_unique<AudioEngine>())
    , m_autosavePath(autosavePath.isEmpty()
          ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
              + QStringLiteral("/prototype-autosave.json")
          : autosavePath)
    , m_audioEnabled(audioEnabled)
{
    m_audio->setParent(nullptr);
    if (QFileInfo::exists(m_autosavePath)
        && !ProjectStore::load(m_autosavePath, m_composition)) {
        m_loadFailed = true;
        ensureRecoveryCopy();
    }
    connect(m_audio.get(), &AudioEngine::playingChanged, this, [this] {
        if (!m_audio->playing()) {
            m_compositionPlaybackActive = false;
        }
        emit playingChanged();
    });
    connect(m_audio.get(), &AudioEngine::errorChanged,
            this, &AppController::audioFailedChanged);
}

CompositionModel *AppController::composition() noexcept
{
    return &m_composition;
}

QString AppController::selectedKind() const
{
    return m_selectedKind;
}

int AppController::selectedSound() const noexcept
{
    return m_selectedSound;
}

bool AppController::loopEnabled() const noexcept
{
    return m_loopEnabled;
}

bool AppController::playing() const noexcept
{
    return m_audio->playing();
}

bool AppController::loadFailed() const noexcept
{
    return m_loadFailed;
}

QString AppController::loadFailureMessage() const
{
    return m_loadFailed ? tr("This song needs help. Ask a grown-up.") : QString{};
}

bool AppController::saveFailed() const noexcept
{
    return m_saveFailed;
}

QString AppController::saveFailureMessage() const
{
    return m_saveFailed ? tr("Your song is here, but it is not saved yet.") : QString{};
}

bool AppController::audioFailed() const noexcept
{
    return m_audio->hasError();
}

QString AppController::audioFailureMessage() const
{
    return audioFailed() ? tr("Sound stopped. You can try Play again.") : QString{};
}

bool AppController::pitchedPlacementRejected() const noexcept
{
    return m_pitchedPlacementRejected;
}

void AppController::setPitchedPlacementRejected(bool rejected)
{
    if (m_pitchedPlacementRejected == rejected) {
        return;
    }
    m_pitchedPlacementRejected = rejected;
    emit pitchedPlacementRejectedChanged();
}

void AppController::selectPitched(int soundId)
{
    if (soundId < 0 || soundId >= 4) {
        return;
    }
    m_selectedKind = QStringLiteral("pitched");
    m_selectedSound = soundId;
    emit selectionChanged();
    if (m_audioEnabled) {
        if (m_audio->play(AudioRenderer::renderPitched(soundId, 3, 180))) {
            m_compositionPlaybackActive = false;
        }
    }
}

void AppController::selectPercussion(int soundId)
{
    if (soundId < 0 || soundId >= 2) {
        return;
    }
    m_selectedKind = QStringLiteral("percussion");
    m_selectedSound = soundId;
    emit selectionChanged();
    if (m_audioEnabled) {
        if (m_audio->play(AudioRenderer::renderPercussion(soundId, 180))) {
            m_compositionPlaybackActive = false;
        }
    }
}

bool AppController::placePitched(int step, int row)
{
    setPitchedPlacementRejected(false);
    if (m_loadFailed || m_selectedKind != QStringLiteral("pitched")
        || step < 0 || step >= m_composition.measureCount() * m_composition.stepsPerMeasure()
        || row < 0 || row >= 7) {
        return false;
    }
    if (!m_composition.placePitched(step, row, m_selectedSound)) {
        setPitchedPlacementRejected(true);
        return false;
    }
    if (m_audioEnabled) {
        if (m_audio->play(AudioRenderer::renderPitched(m_selectedSound, row, 220))) {
            m_compositionPlaybackActive = false;
        }
    }
    return save();
}

bool AppController::placePercussion(int step)
{
    if (m_loadFailed || m_selectedKind != QStringLiteral("percussion")
        || !m_composition.placePercussion(step, m_selectedSound)) {
        return false;
    }
    if (m_audioEnabled) {
        if (m_audio->play(AudioRenderer::renderPercussion(m_selectedSound, 180))) {
            m_compositionPlaybackActive = false;
        }
    }
    return save();
}

bool AppController::eraseAt(const QString &kind, int step, int row)
{
    if (m_loadFailed || !m_composition.eraseAt(kind, step, row)) {
        return false;
    }
    return save();
}

bool AppController::addMeasure()
{
    if (m_loadFailed || !m_composition.addMeasure()) {
        return false;
    }
    return save();
}

bool AppController::undo()
{
    if (m_loadFailed || !m_composition.undo()) {
        return false;
    }
    return save();
}

bool AppController::preserveFailedAutosaveAndStartNew()
{
    if (!m_loadFailed) {
        return false;
    }

    if (!ensureRecoveryCopy()) {
        return false;
    }

    CompositionModel freshComposition;
    if (!m_composition.loadJson(freshComposition.toJson())) {
        return false;
    }
    if (!save()) {
        return false;
    }

    m_loadFailed = false;
    emit loadFailedChanged();
    return true;
}

void AppController::play()
{
    if (m_audioEnabled
        && m_audio->play(AudioRenderer::renderComposition(m_composition.toJson(), 112),
                         m_loopEnabled)) {
        m_compositionPlaybackActive = true;
    }
}

void AppController::stop()
{
    m_compositionPlaybackActive = false;
    m_audio->stop();
}

void AppController::setLoopEnabled(bool enabled)
{
    if (m_loopEnabled == enabled) {
        return;
    }
    m_loopEnabled = enabled;
    if (m_compositionPlaybackActive) {
        m_audio->setLoopEnabled(enabled);
    }
    emit loopEnabledChanged();
}

bool AppController::save()
{
    const bool succeeded = ProjectStore::save(m_autosavePath, m_composition);
    const bool failed = !succeeded;
    if (m_saveFailed != failed) {
        m_saveFailed = failed;
        emit saveFailedChanged();
    }
    return succeeded;
}

bool AppController::ensureRecoveryCopy()
{
    if (m_recoveryPath.isEmpty()) {
        m_recoveryPath = m_autosavePath + QStringLiteral(".recovery-")
            + QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    if (!QFileInfo::exists(m_recoveryPath)
        && !QFile::copy(m_autosavePath, m_recoveryPath)) {
        return false;
    }

    const QByteArray sourceHash = fileSha256(m_autosavePath);
    const QByteArray recoveryHash = fileSha256(m_recoveryPath);
    return !sourceHash.isEmpty() && sourceHash == recoveryHash;
}
