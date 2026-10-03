#include <QtTest>
#include <QTemporaryDir>

#include "CompositionModel.h"
#include "ProjectStore.h"

class ProjectStoreTest : public QObject
{
    Q_OBJECT

private slots:
    void savesAndLoadsComposition();
};

void ProjectStoreTest::savesAndLoadsComposition()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath("autosave.json");

    CompositionModel source;
    QVERIFY(source.addMeasure());
    QVERIFY(source.placePitched(5, 6, 3));
    QVERIFY(source.placePercussion(2, 1));
    QVERIFY(ProjectStore::save(path, source));

    CompositionModel loaded;
    QVERIFY(ProjectStore::load(path, loaded));
    QCOMPARE(loaded.measureCount(), 3);
    QCOMPARE(loaded.rowCount(), 2);
    QCOMPARE(loaded.data(loaded.index(0), CompositionModel::SoundIdRole).toInt(), 3);
    QCOMPARE(loaded.data(loaded.index(1), CompositionModel::KindRole).toString(),
             "percussion");
}

QTEST_MAIN(ProjectStoreTest)
#include "tst_projectstore.moc"
