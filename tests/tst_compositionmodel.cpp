#include <QtTest>
#include <QJsonArray>

#include "CompositionModel.h"

namespace {
QJsonObject token(const QString &id, const QString &kind, int step, int row, int sound)
{
    return {
        {QStringLiteral("id"), id},
        {QStringLiteral("kind"), kind},
        {QStringLiteral("step"), step},
        {QStringLiteral("row"), row},
        {QStringLiteral("sound"), sound},
    };
}

QJsonObject composition(const QJsonArray &tokens)
{
    return {
        {QStringLiteral("version"), 1},
        {QStringLiteral("measures"), 2},
        {QStringLiteral("tokens"), tokens},
    };
}
}

class CompositionModelTest : public QObject
{
    Q_OBJECT

private slots:
    void startsWithTwoEmptyMeasures();
    void growsToEightMeasuresButNoFurther();
    void refusesToRemoveEitherMinimumMeasure();
    void removesFinalMeasureEventsAsOneExactlyUndoableEdit();
    void clearsSongAsOneExactlyUndoableEdit();
    void placesPitchedTokenWithItsOwnSound();
    void rejectsOutOfRangePitchedPlacement();
    void placesPercussionInSeparateKind();
    void replacesOccupiedCellWithoutChangingOtherTokens();
    void erasesAndUndoesAPlacedToken();
    void limitsSimultaneousPitchedSoundsToThree();
    void replacesPercussionAtTheSameStepAndRow();
    void undoRestoresReplacedPitchedToken();
    void undoRestoresReplacedPercussionToken();
    void undoesSeveralEditsInReverseOrder();
    void newEditAfterUndoPreservesEarlierHistory();
    void boundsUndoHistoryToOneHundredEdits();
    void loadingCompositionStartsFreshUndoHistory();
    void rejectsDuplicateTokenIdsOnLoad();
    void rejectsDuplicatePitchedOccupancyOnLoad();
    void rejectsDuplicatePercussionOccupancyOnLoad();
    void rejectsMoreThanThreePitchedEventsAtOneStepOnLoad();
    void rejectsMoreThanTwoPercussionEventsAtOneStepOnLoad();
    void rejectedJsonLeavesCompositionUnchanged();
};

void CompositionModelTest::startsWithTwoEmptyMeasures()
{
    CompositionModel model;

    QCOMPARE(model.measureCount(), 2);
    QCOMPARE(model.stepsPerMeasure(), 4);
    QCOMPARE(model.rowCount(), 0);
}

void CompositionModelTest::growsToEightMeasuresButNoFurther()
{
    CompositionModel model;

    for (int expected = 3; expected <= 8; ++expected) {
        QVERIFY(model.addMeasure());
        QCOMPARE(model.measureCount(), expected);
    }

    QVERIFY(!model.addMeasure());
    QCOMPARE(model.measureCount(), 8);
}

void CompositionModelTest::refusesToRemoveEitherMinimumMeasure()
{
    CompositionModel model;

    QVERIFY(!model.removeLastMeasure());
    QCOMPARE(model.measureCount(), 2);
    QVERIFY(!model.canUndo());
}

void CompositionModelTest::removesFinalMeasureEventsAsOneExactlyUndoableEdit()
{
    CompositionModel model;
    QVERIFY(model.addMeasure());
    QVERIFY(model.placePitched(2, 1, 0));
    QVERIFY(model.placePitched(8, 6, 3));
    QVERIFY(model.placePercussion(11, 1));
    const QJsonObject beforeRemoval = model.toJson();

    QVERIFY(model.removeLastMeasure());
    QCOMPARE(model.measureCount(), 2);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), CompositionModel::StepRole).toInt(), 2);

    QVERIFY(model.undo());
    QCOMPARE(model.toJson(), beforeRemoval);
}

void CompositionModelTest::clearsSongAsOneExactlyUndoableEdit()
{
    CompositionModel model;
    QVERIFY(model.addMeasure());
    QVERIFY(model.placePitched(2, 1, 0));
    QVERIFY(model.placePitched(8, 6, 3));
    QVERIFY(model.placePercussion(11, 1));
    const QJsonObject beforeClear = model.toJson();

    QVERIFY(model.clearSong());
    QCOMPARE(model.measureCount(), 2);
    QCOMPARE(model.rowCount(), 0);

    QVERIFY(model.undo());
    QCOMPARE(model.toJson(), beforeClear);
}

void CompositionModelTest::placesPitchedTokenWithItsOwnSound()
{
    CompositionModel model;

    QVERIFY(model.placePitched(3, 5, 2));
    QCOMPARE(model.rowCount(), 1);
    const QModelIndex token = model.index(0);
    QCOMPARE(model.data(token, CompositionModel::KindRole).toString(), "pitched");
    QCOMPARE(model.data(token, CompositionModel::StepRole).toInt(), 3);
    QCOMPARE(model.data(token, CompositionModel::PitchRowRole).toInt(), 5);
    QCOMPARE(model.data(token, CompositionModel::SoundIdRole).toInt(), 2);
}

void CompositionModelTest::rejectsOutOfRangePitchedPlacement()
{
    CompositionModel model;

    QVERIFY(!model.placePitched(-1, 0, 0));
    QVERIFY(!model.placePitched(8, 0, 0));
    QVERIFY(!model.placePitched(0, -1, 0));
    QVERIFY(!model.placePitched(0, 7, 0));
    QVERIFY(!model.placePitched(0, 0, -1));
    QVERIFY(!model.placePitched(0, 0, 4));
    QCOMPARE(model.rowCount(), 0);
}

void CompositionModelTest::placesPercussionInSeparateKind()
{
    CompositionModel model;

    QVERIFY(model.placePercussion(4, 1));
    QCOMPARE(model.rowCount(), 1);
    const QModelIndex token = model.index(0);
    QCOMPARE(model.data(token, CompositionModel::KindRole).toString(), "percussion");
    QCOMPARE(model.data(token, CompositionModel::StepRole).toInt(), 4);
    QCOMPARE(model.data(token, CompositionModel::PitchRowRole).toInt(), 1);
    QCOMPARE(model.data(token, CompositionModel::SoundIdRole).toInt(), 1);
}

void CompositionModelTest::replacesOccupiedCellWithoutChangingOtherTokens()
{
    CompositionModel model;

    QVERIFY(model.placePitched(2, 4, 0));
    QVERIFY(model.placePitched(2, 4, 3));

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), CompositionModel::SoundIdRole).toInt(), 3);
}

void CompositionModelTest::erasesAndUndoesAPlacedToken()
{
    CompositionModel model;

    QVERIFY(model.placePitched(1, 2, 3));
    QVERIFY(model.eraseAt("pitched", 1, 2));
    QCOMPARE(model.rowCount(), 0);

    QVERIFY(model.undo());
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), CompositionModel::SoundIdRole).toInt(), 3);
}

void CompositionModelTest::limitsSimultaneousPitchedSoundsToThree()
{
    CompositionModel model;

    QVERIFY(model.placePitched(0, 0, 0));
    QVERIFY(model.placePitched(0, 2, 1));
    QVERIFY(model.placePitched(0, 4, 2));
    QVERIFY(!model.placePitched(0, 6, 3));
    QCOMPARE(model.rowCount(), 3);
}

void CompositionModelTest::replacesPercussionAtTheSameStepAndRow()
{
    CompositionModel model;

    QVERIFY(model.placePercussion(3, 1));
    QVERIFY(model.placePercussion(3, 1));
    QCOMPARE(model.rowCount(), 1);
}

void CompositionModelTest::undoRestoresReplacedPitchedToken()
{
    CompositionModel model;

    QVERIFY(model.placePitched(2, 4, 0));
    const QString originalId = model.data(model.index(0), CompositionModel::IdRole).toString();

    QVERIFY(model.placePitched(2, 4, 3));
    QVERIFY(model.data(model.index(0), CompositionModel::IdRole).toString() != originalId);
    QCOMPARE(model.data(model.index(0), CompositionModel::SoundIdRole).toInt(), 3);

    QVERIFY(model.undo());
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), CompositionModel::IdRole).toString(), originalId);
    QCOMPARE(model.data(model.index(0), CompositionModel::KindRole).toString(), "pitched");
    QCOMPARE(model.data(model.index(0), CompositionModel::StepRole).toInt(), 2);
    QCOMPARE(model.data(model.index(0), CompositionModel::PitchRowRole).toInt(), 4);
    QCOMPARE(model.data(model.index(0), CompositionModel::SoundIdRole).toInt(), 0);
}

void CompositionModelTest::undoRestoresReplacedPercussionToken()
{
    CompositionModel model;

    QVERIFY(model.placePercussion(3, 1));
    const QString originalId = model.data(model.index(0), CompositionModel::IdRole).toString();

    QVERIFY(model.placePercussion(3, 1));
    QVERIFY(model.data(model.index(0), CompositionModel::IdRole).toString() != originalId);

    QVERIFY(model.undo());
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), CompositionModel::IdRole).toString(), originalId);
    QCOMPARE(model.data(model.index(0), CompositionModel::KindRole).toString(), "percussion");
    QCOMPARE(model.data(model.index(0), CompositionModel::StepRole).toInt(), 3);
    QCOMPARE(model.data(model.index(0), CompositionModel::PitchRowRole).toInt(), 1);
    QCOMPARE(model.data(model.index(0), CompositionModel::SoundIdRole).toInt(), 1);
}

void CompositionModelTest::undoesSeveralEditsInReverseOrder()
{
    CompositionModel model;

    QVERIFY(model.placePitched(0, 0, 0));
    QVERIFY(model.placePitched(1, 1, 1));
    QVERIFY(model.placePercussion(2, 0));
    QCOMPARE(model.rowCount(), 3);

    QVERIFY(model.undo());
    QCOMPARE(model.rowCount(), 2);
    QVERIFY(model.canUndo());

    QVERIFY(model.undo());
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), CompositionModel::StepRole).toInt(), 0);
    QVERIFY(model.canUndo());

    QVERIFY(model.undo());
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(!model.canUndo());
    QVERIFY(!model.undo());
}

void CompositionModelTest::newEditAfterUndoPreservesEarlierHistory()
{
    CompositionModel model;

    QVERIFY(model.placePitched(0, 0, 0));
    QVERIFY(model.placePitched(1, 1, 1));

    QVERIFY(model.undo());
    QCOMPARE(model.rowCount(), 1);

    QVERIFY(model.placePercussion(2, 0));
    QCOMPARE(model.rowCount(), 2);

    QVERIFY(model.undo());
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0), CompositionModel::KindRole).toString(), "pitched");
    QCOMPARE(model.data(model.index(0), CompositionModel::StepRole).toInt(), 0);

    QVERIFY(model.undo());
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(!model.canUndo());
}

void CompositionModelTest::boundsUndoHistoryToOneHundredEdits()
{
    CompositionModel model;

    QVERIFY(model.placePitched(0, 0, 0));
    for (int edit = 0; edit < 100; ++edit) {
        QVERIFY(model.placePitched(0, 0, edit % 4));
    }

    for (int undo = 0; undo < 100; ++undo) {
        QVERIFY(model.undo());
    }

    QCOMPARE(model.rowCount(), 1);
    QVERIFY(!model.canUndo());
    QVERIFY(!model.undo());
}

void CompositionModelTest::loadingCompositionStartsFreshUndoHistory()
{
    CompositionModel model;
    CompositionModel emptyModel;

    QVERIFY(model.placePitched(0, 0, 0));
    QVERIFY(model.canUndo());

    QVERIFY(model.loadJson(emptyModel.toJson()));
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(!model.canUndo());
    QVERIFY(!model.undo());
}

void CompositionModelTest::rejectsDuplicateTokenIdsOnLoad()
{
    CompositionModel model;
    const QJsonArray tokens{
        token("same-id", "pitched", 0, 0, 0),
        token("same-id", "pitched", 1, 1, 1),
    };

    QVERIFY(!model.loadJson(composition(tokens)));
}

void CompositionModelTest::rejectsDuplicatePitchedOccupancyOnLoad()
{
    CompositionModel model;
    const QJsonArray tokens{
        token("first", "pitched", 0, 2, 0),
        token("second", "pitched", 0, 2, 3),
    };

    QVERIFY(!model.loadJson(composition(tokens)));
}

void CompositionModelTest::rejectsDuplicatePercussionOccupancyOnLoad()
{
    CompositionModel model;
    const QJsonArray tokens{
        token("first", "percussion", 0, 1, 1),
        token("second", "percussion", 0, 1, 1),
    };

    QVERIFY(!model.loadJson(composition(tokens)));
}

void CompositionModelTest::rejectsMoreThanThreePitchedEventsAtOneStepOnLoad()
{
    CompositionModel model;
    const QJsonArray tokens{
        token("one", "pitched", 0, 0, 0),
        token("two", "pitched", 0, 1, 1),
        token("three", "pitched", 0, 2, 2),
        token("four", "pitched", 0, 3, 3),
    };

    QVERIFY(!model.loadJson(composition(tokens)));
}

void CompositionModelTest::rejectsMoreThanTwoPercussionEventsAtOneStepOnLoad()
{
    CompositionModel model;
    // Only two percussion rows exist, so any third event must also violate
    // row validity or duplicate occupancy. This fixture uses the latter.
    const QJsonArray tokens{
        token("one", "percussion", 0, 0, 0),
        token("two", "percussion", 0, 1, 1),
        token("three", "percussion", 0, 0, 0),
    };

    QVERIFY(!model.loadJson(composition(tokens)));
}

void CompositionModelTest::rejectedJsonLeavesCompositionUnchanged()
{
    CompositionModel model;
    QVERIFY(model.addMeasure());
    QVERIFY(model.placePitched(5, 6, 3));
    const QJsonObject original = model.toJson();
    const bool originalCanUndo = model.canUndo();
    const QJsonArray invalidTokens{
        token("same-id", "pitched", 0, 0, 0),
        token("same-id", "pitched", 1, 1, 1),
    };

    QVERIFY(!model.loadJson(composition(invalidTokens)));
    QCOMPARE(model.toJson(), original);
    QCOMPARE(model.canUndo(), originalCanUndo);

    QVERIFY(model.undo());
    QCOMPARE(model.measureCount(), 3);
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(model.canUndo());

    QVERIFY(model.undo());
    QCOMPARE(model.measureCount(), 2);
    QCOMPARE(model.rowCount(), 0);
    QVERIFY(!model.canUndo());
}

QTEST_MAIN(CompositionModelTest)
#include "tst_compositionmodel.moc"
