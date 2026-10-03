#pragma once

#include "AudioEngine.h"
#include "CompositionModel.h"

#include <QObject>
#include <QString>

#include <memory>

class AppController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(CompositionModel *composition READ composition CONSTANT)
    Q_PROPERTY(QString selectedKind READ selectedKind NOTIFY selectionChanged)
    Q_PROPERTY(int selectedSound READ selectedSound NOTIFY selectionChanged)
    Q_PROPERTY(bool loopEnabled READ loopEnabled WRITE setLoopEnabled NOTIFY loopEnabledChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(bool loadFailed READ loadFailed NOTIFY loadFailedChanged)
    Q_PROPERTY(QString loadFailureMessage READ loadFailureMessage NOTIFY loadFailedChanged)
    Q_PROPERTY(bool saveFailed READ saveFailed NOTIFY saveFailedChanged)
    Q_PROPERTY(QString saveFailureMessage READ saveFailureMessage NOTIFY saveFailedChanged)
    Q_PROPERTY(bool audioFailed READ audioFailed NOTIFY audioFailedChanged)
    Q_PROPERTY(QString audioFailureMessage READ audioFailureMessage NOTIFY audioFailedChanged)

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
    [[nodiscard]] bool loadFailed() const noexcept;
    [[nodiscard]] QString loadFailureMessage() const;
    [[nodiscard]] bool saveFailed() const noexcept;
    [[nodiscard]] QString saveFailureMessage() const;
    [[nodiscard]] bool audioFailed() const noexcept;
    [[nodiscard]] QString audioFailureMessage() const;

    Q_INVOKABLE void selectPitched(int soundId);
    Q_INVOKABLE void selectPercussion(int soundId);
    Q_INVOKABLE bool placePitched(int step, int row);
    Q_INVOKABLE bool placePercussion(int step);
    Q_INVOKABLE bool eraseAt(const QString &kind, int step, int row);
    Q_INVOKABLE bool addMeasure();
    Q_INVOKABLE bool undo();
    Q_INVOKABLE bool preserveFailedAutosaveAndStartNew();
    Q_INVOKABLE void play();
    Q_INVOKABLE void stop();
    void setLoopEnabled(bool enabled);

signals:
    void selectionChanged();
    void loopEnabledChanged();
    void playingChanged();
    void loadFailedChanged();
    void saveFailedChanged();
    void audioFailedChanged();

private:
    bool save();
    bool ensureRecoveryCopy();

    CompositionModel m_composition;
    std::unique_ptr<AudioEngine> m_audio;
    QString m_autosavePath;
    QString m_selectedKind = QStringLiteral("pitched");
    int m_selectedSound = 0;
    bool m_loopEnabled = false;
    bool m_audioEnabled = true;
    bool m_loadFailed = false;
    bool m_saveFailed = false;
    bool m_compositionPlaybackActive = false;
    QString m_recoveryPath;
};
