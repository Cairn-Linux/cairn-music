#pragma once

#include "AudioEngine.h"
#include "CompositionModel.h"

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>

class AppController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(CompositionModel *composition READ composition CONSTANT)
    Q_PROPERTY(QString selectedKind READ selectedKind NOTIFY selectionChanged)
    Q_PROPERTY(int selectedSound READ selectedSound NOTIFY selectionChanged)
    Q_PROPERTY(bool loopEnabled READ loopEnabled WRITE setLoopEnabled NOTIFY loopEnabledChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(int playbackStep READ playbackStep NOTIFY playbackProgressChanged)
    Q_PROPERTY(int playbackCycle READ playbackCycle NOTIFY playbackProgressChanged)
    Q_PROPERTY(QString playbackStatus READ playbackStatus NOTIFY playbackProgressChanged)
    Q_PROPERTY(bool loadFailed READ loadFailed NOTIFY loadFailedChanged)
    Q_PROPERTY(QString loadFailureMessage READ loadFailureMessage NOTIFY loadFailedChanged)
    Q_PROPERTY(bool saveFailed READ saveFailed NOTIFY saveFailedChanged)
    Q_PROPERTY(QString saveFailureMessage READ saveFailureMessage NOTIFY saveFailedChanged)
    Q_PROPERTY(bool audioFailed READ audioFailed NOTIFY audioFailedChanged)
    Q_PROPERTY(QString audioFailureMessage READ audioFailureMessage NOTIFY audioFailedChanged)
    Q_PROPERTY(bool pitchedPlacementRejected READ pitchedPlacementRejected
               NOTIFY pitchedPlacementRejectedChanged)

public:
    explicit AppController(const QString &autosavePath = {}, bool audioEnabled = true,
                           QObject *parent = nullptr);
    AppController(const QString &autosavePath, bool audioEnabled,
                  std::unique_ptr<AudioEngine> audio, QObject *parent = nullptr);

    [[nodiscard]] CompositionModel *composition() noexcept;
    [[nodiscard]] QString selectedKind() const;
    [[nodiscard]] int selectedSound() const noexcept;
    [[nodiscard]] bool loopEnabled() const noexcept;
    [[nodiscard]] bool playing() const noexcept;
    [[nodiscard]] int playbackStep() const noexcept;
    [[nodiscard]] int playbackCycle() const noexcept;
    [[nodiscard]] QString playbackStatus() const;
    [[nodiscard]] bool loadFailed() const noexcept;
    [[nodiscard]] QString loadFailureMessage() const;
    [[nodiscard]] bool saveFailed() const noexcept;
    [[nodiscard]] QString saveFailureMessage() const;
    [[nodiscard]] bool audioFailed() const noexcept;
    [[nodiscard]] QString audioFailureMessage() const;
    [[nodiscard]] bool pitchedPlacementRejected() const noexcept;

    Q_INVOKABLE void selectPitched(int soundId);
    Q_INVOKABLE void selectPercussion(int soundId);
    Q_INVOKABLE bool placePitched(int step, int row);
    Q_INVOKABLE bool placePercussion(int step);
    Q_INVOKABLE bool eraseAt(const QString &kind, int step, int row);
    Q_INVOKABLE bool addMeasure();
    Q_INVOKABLE bool removeMeasure();
    Q_INVOKABLE bool undo();
    Q_INVOKABLE bool preserveFailedAutosaveAndStartNew();
    Q_INVOKABLE void play();
    Q_INVOKABLE void stop();
    void setLoopEnabled(bool enabled);

signals:
    void selectionChanged();
    void loopEnabledChanged();
    void playingChanged();
    void playbackProgressChanged();
    void loopRestarted();
    void loadFailedChanged();
    void saveFailedChanged();
    void audioFailedChanged();
    void pitchedPlacementRejectedChanged();

private:
    bool save();
    bool ensureRecoveryCopy();
    void setPitchedPlacementRejected(bool rejected);
    void stopCompositionPlaybackForMutation();
    void startPlaybackProgress();
    void resetPlaybackProgress();
    void refreshPlaybackProgress();

    CompositionModel m_composition;
    std::unique_ptr<AudioEngine> m_audio;
    QString m_autosavePath;
    QString m_selectedKind = QStringLiteral("pitched");
    int m_selectedSound = 0;
    bool m_loopEnabled = false;
    bool m_audioEnabled = true;
    bool m_loadFailed = false;
    bool m_saveFailed = false;
    bool m_pitchedPlacementRejected = false;
    bool m_compositionPlaybackActive = false;
    int m_playbackStep = -1;
    int m_playbackCycle = 0;
    QElapsedTimer m_playbackElapsed;
    QTimer m_playbackTimer;
    QString m_recoveryPath;
};
