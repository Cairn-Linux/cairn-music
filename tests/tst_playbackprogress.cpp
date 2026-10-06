#include <QtTest>

#include "PlaybackProgress.h"

class PlaybackProgressTest : public QObject
{
    Q_OBJECT

private slots:
    void identifiesCurrentStep();
    void identifiesLoopRestart();
};

void PlaybackProgressTest::identifiesCurrentStep()
{
    const PlaybackProgress::State beforeStart = PlaybackProgress::stateAt(-1, 8, 500, false);
    QCOMPARE(beforeStart.step, -1);

    const PlaybackProgress::State first = PlaybackProgress::stateAt(0, 8, 500, false);
    QCOMPARE(first.step, 0);

    const PlaybackProgress::State fourth = PlaybackProgress::stateAt(1750, 8, 500, false);
    QCOMPARE(fourth.step, 3);

    const PlaybackProgress::State finished = PlaybackProgress::stateAt(4000, 8, 500, false);
    QCOMPARE(finished.step, -1);
}

void PlaybackProgressTest::identifiesLoopRestart()
{
    const PlaybackProgress::State lastBeat = PlaybackProgress::stateAt(3999, 8, 500, true);
    QCOMPARE(lastBeat.step, 7);
    QCOMPARE(lastBeat.cycle, 0);
    QVERIFY(!lastBeat.atLoopStart);

    const PlaybackProgress::State restarted = PlaybackProgress::stateAt(4000, 8, 500, true);
    QCOMPARE(restarted.step, 0);
    QCOMPARE(restarted.cycle, 1);
    QVERIFY(restarted.atLoopStart);
}

QTEST_APPLESS_MAIN(PlaybackProgressTest)
#include "tst_playbackprogress.moc"
