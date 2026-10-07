#pragma once

#include <QAbstractListModel>
#include <QJsonObject>
#include <QString>
#include <QVector>

class CompositionModel final : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int measureCount READ measureCount NOTIFY measureCountChanged)
    Q_PROPERTY(int stepsPerMeasure READ stepsPerMeasure CONSTANT)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY canUndoChanged)

public:
    enum Role {
        IdRole = Qt::UserRole + 1,
        KindRole,
        StepRole,
        PitchRowRole,
        SoundIdRole,
    };
    Q_ENUM(Role)

    explicit CompositionModel(QObject *parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int measureCount() const noexcept;
    [[nodiscard]] int stepsPerMeasure() const noexcept;
    [[nodiscard]] bool canUndo() const noexcept;
    bool addMeasure();
    bool removeLastMeasure();
    bool placePitched(int step, int pitchRow, int soundId);
    bool placePercussion(int step, int soundId);
    bool eraseAt(const QString &kind, int step, int row);
    bool undo();
    [[nodiscard]] QJsonObject toJson() const;
    bool loadJson(const QJsonObject &object);

signals:
    void measureCountChanged();
    void canUndoChanged();

private:
    struct Token {
        QString id;
        QString kind;
        int step = 0;
        int pitchRow = 0;
        int soundId = 0;
    };

    struct UndoState {
        int measureCount = 2;
        QVector<Token> tokens;
    };

    void saveUndoPoint();

    static constexpr int MaxUndoHistory = 100;

    int m_measureCount = 2;
    QVector<Token> m_tokens;
    QVector<UndoState> m_undoHistory;
};
