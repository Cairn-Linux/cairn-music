#include "CompositionModel.h"

#include <QJsonArray>
#include <QHash>
#include <QSet>
#include <QUuid>

CompositionModel::CompositionModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CompositionModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_tokens.size();
}

QVariant CompositionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tokens.size()) {
        return {};
    }

    const Token &token = m_tokens.at(index.row());
    switch (role) {
    case IdRole:
        return token.id;
    case KindRole:
        return token.kind;
    case StepRole:
        return token.step;
    case PitchRowRole:
        return token.pitchRow;
    case SoundIdRole:
        return token.soundId;
    default:
        return {};
    }
}

QHash<int, QByteArray> CompositionModel::roleNames() const
{
    return {
        {IdRole, "tokenId"},
        {KindRole, "kind"},
        {StepRole, "step"},
        {PitchRowRole, "pitchRow"},
        {SoundIdRole, "soundId"},
    };
}

int CompositionModel::measureCount() const noexcept
{
    return m_measureCount;
}

int CompositionModel::stepsPerMeasure() const noexcept
{
    return 4;
}

bool CompositionModel::canUndo() const noexcept
{
    return !m_undoHistory.isEmpty();
}

void CompositionModel::saveUndoPoint()
{
    const bool wasEmpty = m_undoHistory.isEmpty();
    if (m_undoHistory.size() >= MaxUndoHistory) {
        m_undoHistory.removeFirst();
    }
    m_undoHistory.append({m_measureCount, m_tokens});
    if (wasEmpty) {
        emit canUndoChanged();
    }
}

bool CompositionModel::addMeasure()
{
    if (m_measureCount >= 8) {
        return false;
    }
    saveUndoPoint();
    ++m_measureCount;
    emit measureCountChanged();
    return true;
}

bool CompositionModel::removeLastMeasure()
{
    if (m_measureCount <= 2) {
        return false;
    }

    saveUndoPoint();
    const int firstRemovedStep = (m_measureCount - 1) * stepsPerMeasure();
    --m_measureCount;
    int searchRow = m_tokens.size() - 1;
    while (searchRow >= 0) {
        while (searchRow >= 0 && m_tokens.at(searchRow).step < firstRemovedStep) {
            --searchRow;
        }
        if (searchRow < 0) {
            break;
        }

        const int lastRemovedRow = searchRow;
        while (searchRow >= 0 && m_tokens.at(searchRow).step >= firstRemovedStep) {
            --searchRow;
        }
        const int firstRemovedRow = searchRow + 1;
        beginRemoveRows({}, firstRemovedRow, lastRemovedRow);
        m_tokens.remove(firstRemovedRow, lastRemovedRow - firstRemovedRow + 1);
        endRemoveRows();
    }
    emit measureCountChanged();
    return true;
}

bool CompositionModel::clearSong()
{
    if (m_measureCount == 2 && m_tokens.isEmpty()) {
        return false;
    }

    saveUndoPoint();
    const bool measureChanged = m_measureCount != 2;
    beginResetModel();
    m_measureCount = 2;
    m_tokens.clear();
    endResetModel();
    if (measureChanged) {
        emit measureCountChanged();
    }
    return true;
}

CompositionModel::PlacementResult CompositionModel::pitchedPlacementResult(
    int step, int pitchRow, int soundId) const
{
    if (step < 0 || step >= m_measureCount * stepsPerMeasure()
        || pitchRow < 0 || pitchRow >= 7 || soundId < 0 || soundId >= 4) {
        return PlacementResult::Rejected;
    }

    int simultaneous = 0;
    for (const Token &token : m_tokens) {
        if (token.kind != QStringLiteral("pitched") || token.step != step) {
            continue;
        }
        if (token.pitchRow == pitchRow) {
            return token.soundId == soundId
                ? PlacementResult::Unchanged : PlacementResult::Changed;
        }
        ++simultaneous;
    }
    return simultaneous >= 3 ? PlacementResult::Rejected : PlacementResult::Changed;
}

CompositionModel::PlacementResult CompositionModel::placePitchedResult(
    int step, int pitchRow, int soundId)
{
    const PlacementResult result = pitchedPlacementResult(step, pitchRow, soundId);
    if (result != PlacementResult::Changed) {
        return result;
    }

    for (int row = 0; row < m_tokens.size(); ++row) {
        const Token &existing = m_tokens.at(row);
        if (existing.kind == QStringLiteral("pitched")
            && existing.step == step && existing.pitchRow == pitchRow) {
            saveUndoPoint();
            m_tokens[row].id = QUuid::createUuid().toString(QUuid::WithoutBraces);
            m_tokens[row].soundId = soundId;
            emit dataChanged(index(row), index(row), {IdRole, SoundIdRole});
            return PlacementResult::Changed;
        }
    }

    saveUndoPoint();
    const int insertionRow = m_tokens.size();
    beginInsertRows({}, insertionRow, insertionRow);
    m_tokens.append({QUuid::createUuid().toString(QUuid::WithoutBraces),
                     QStringLiteral("pitched"), step, pitchRow, soundId});
    endInsertRows();
    return PlacementResult::Changed;
}

bool CompositionModel::placePitched(int step, int pitchRow, int soundId)
{
    return placePitchedResult(step, pitchRow, soundId) != PlacementResult::Rejected;
}

CompositionModel::PlacementResult CompositionModel::percussionPlacementResult(
    int step, int soundId) const
{
    if (step < 0 || step >= m_measureCount * stepsPerMeasure()
        || soundId < 0 || soundId >= 2) {
        return PlacementResult::Rejected;
    }

    for (const Token &token : m_tokens) {
        if (token.kind == QStringLiteral("percussion")
            && token.step == step && token.pitchRow == soundId) {
            return PlacementResult::Unchanged;
        }
    }
    return PlacementResult::Changed;
}

CompositionModel::PlacementResult CompositionModel::placePercussionResult(
    int step, int soundId)
{
    const PlacementResult result = percussionPlacementResult(step, soundId);
    if (result != PlacementResult::Changed) {
        return result;
    }

    saveUndoPoint();
    const int insertionRow = m_tokens.size();
    beginInsertRows({}, insertionRow, insertionRow);
    m_tokens.append({QUuid::createUuid().toString(QUuid::WithoutBraces),
                     QStringLiteral("percussion"), step, soundId, soundId});
    endInsertRows();
    return PlacementResult::Changed;
}

bool CompositionModel::placePercussion(int step, int soundId)
{
    return placePercussionResult(step, soundId) != PlacementResult::Rejected;
}

bool CompositionModel::hasTokenAt(const QString &kind, int step, int row) const
{
    for (const Token &token : m_tokens) {
        if (token.kind == kind && token.step == step && token.pitchRow == row) {
            return true;
        }
    }
    return false;
}

bool CompositionModel::eraseAt(const QString &kind, int step, int row)
{
    for (int tokenIndex = 0; tokenIndex < m_tokens.size(); ++tokenIndex) {
        const Token &token = m_tokens.at(tokenIndex);
        if (token.kind == kind && token.step == step && token.pitchRow == row) {
            saveUndoPoint();
            beginRemoveRows({}, tokenIndex, tokenIndex);
            m_tokens.removeAt(tokenIndex);
            endRemoveRows();
            return true;
        }
    }
    return false;
}

bool CompositionModel::undo()
{
    if (m_undoHistory.isEmpty()) {
        return false;
    }

    const UndoState state = m_undoHistory.takeLast();
    const bool measureChanged = m_measureCount != state.measureCount;
    m_measureCount = state.measureCount;
    if (measureChanged) {
        emit measureCountChanged();
    }
    restoreTokens(state.tokens);
    if (m_undoHistory.isEmpty()) {
        emit canUndoChanged();
    }
    return true;
}

void CompositionModel::restoreTokens(const QVector<Token> &tokens)
{
    QSet<QString> restoredIds;
    for (const Token &token : tokens) {
        restoredIds.insert(token.id);
    }

    const QList<int> allRoles{IdRole, KindRole, StepRole, PitchRowRole, SoundIdRole};
    for (int restoredRow = 0; restoredRow < tokens.size(); ++restoredRow) {
        const Token &restored = tokens.at(restoredRow);
        if (restoredRow < m_tokens.size() && m_tokens.at(restoredRow).id == restored.id) {
            const Token &current = m_tokens.at(restoredRow);
            if (current.kind != restored.kind || current.step != restored.step
                || current.pitchRow != restored.pitchRow || current.soundId != restored.soundId) {
                m_tokens[restoredRow] = restored;
                emit dataChanged(index(restoredRow), index(restoredRow), allRoles);
            }
            continue;
        }

        int existingRow = -1;
        for (int row = restoredRow + 1; row < m_tokens.size(); ++row) {
            if (m_tokens.at(row).id == restored.id) {
                existingRow = row;
                break;
            }
        }
        if (existingRow >= 0) {
            beginMoveRows({}, existingRow, existingRow, {}, restoredRow);
            m_tokens.move(existingRow, restoredRow);
            endMoveRows();
            continue;
        }

        if (restoredRow < m_tokens.size()
            && !restoredIds.contains(m_tokens.at(restoredRow).id)) {
            m_tokens[restoredRow] = restored;
            emit dataChanged(index(restoredRow), index(restoredRow), allRoles);
            continue;
        }

        beginInsertRows({}, restoredRow, restoredRow);
        m_tokens.insert(restoredRow, restored);
        endInsertRows();
    }

    if (m_tokens.size() > tokens.size()) {
        beginRemoveRows({}, tokens.size(), m_tokens.size() - 1);
        m_tokens.remove(tokens.size(), m_tokens.size() - tokens.size());
        endRemoveRows();
    }
}

QJsonObject CompositionModel::toJson() const
{
    QJsonArray tokens;
    for (const Token &token : m_tokens) {
        tokens.append(QJsonObject{
            {QStringLiteral("id"), token.id},
            {QStringLiteral("kind"), token.kind},
            {QStringLiteral("step"), token.step},
            {QStringLiteral("row"), token.pitchRow},
            {QStringLiteral("sound"), token.soundId},
        });
    }
    return {
        {QStringLiteral("version"), 1},
        {QStringLiteral("measures"), m_measureCount},
        {QStringLiteral("tokens"), tokens},
    };
}

bool CompositionModel::loadJson(const QJsonObject &object)
{
    if (object.value(QStringLiteral("version")).toInt(-1) != 1) {
        return false;
    }
    const int measures = object.value(QStringLiteral("measures")).toInt(-1);
    if (measures < 2 || measures > 8 || !object.value(QStringLiteral("tokens")).isArray()) {
        return false;
    }

    QVector<Token> loaded;
    QSet<QString> tokenIds;
    QSet<int> pitchedOccupancy;
    QSet<int> percussionOccupancy;
    QHash<int, int> pitchedAtStep;
    QHash<int, int> percussionAtStep;
    const QJsonArray tokens = object.value(QStringLiteral("tokens")).toArray();
    if (tokens.size() > measures * stepsPerMeasure() * 9) {
        return false;
    }
    for (const QJsonValue &value : tokens) {
        const QJsonObject item = value.toObject();
        Token token{item.value(QStringLiteral("id")).toString(),
                    item.value(QStringLiteral("kind")).toString(),
                    item.value(QStringLiteral("step")).toInt(-1),
                    item.value(QStringLiteral("row")).toInt(-1),
                    item.value(QStringLiteral("sound")).toInt(-1)};
        const bool pitched = token.kind == QStringLiteral("pitched")
            && token.pitchRow >= 0 && token.pitchRow < 7
            && token.soundId >= 0 && token.soundId < 4;
        const bool percussion = token.kind == QStringLiteral("percussion")
            && token.pitchRow >= 0 && token.pitchRow < 2
            && token.soundId == token.pitchRow;
        if (token.id.isEmpty() || token.step < 0
            || token.step >= measures * stepsPerMeasure()
            || (!pitched && !percussion)) {
            return false;
        }

        if (tokenIds.contains(token.id)) {
            return false;
        }
        tokenIds.insert(token.id);

        if (pitched) {
            const int cell = token.step * 7 + token.pitchRow;
            if (pitchedOccupancy.contains(cell) || ++pitchedAtStep[token.step] > 3) {
                return false;
            }
            pitchedOccupancy.insert(cell);
        } else {
            const int cell = token.step * 2 + token.pitchRow;
            if (percussionOccupancy.contains(cell) || ++percussionAtStep[token.step] > 2) {
                return false;
            }
            percussionOccupancy.insert(cell);
        }
        loaded.append(token);
    }

    beginResetModel();
    const bool measureChanged = m_measureCount != measures;
    const bool hadUndoHistory = !m_undoHistory.isEmpty();
    m_measureCount = measures;
    m_tokens = loaded;
    m_undoHistory.clear();
    endResetModel();
    if (measureChanged) {
        emit measureCountChanged();
    }
    if (hadUndoHistory) {
        emit canUndoChanged();
    }
    return true;
}
