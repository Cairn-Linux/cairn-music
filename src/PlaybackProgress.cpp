#include "PlaybackProgress.h"

PlaybackProgress::State PlaybackProgress::stateAt(long long elapsedMilliseconds,
                                                   int totalSteps,
                                                   int beatMilliseconds,
                                                   bool loopEnabled) noexcept
{
    if (elapsedMilliseconds < 0 || totalSteps <= 0 || beatMilliseconds <= 0) {
        return {};
    }

    const long long songMilliseconds = static_cast<long long>(totalSteps) * beatMilliseconds;
    if (!loopEnabled && elapsedMilliseconds >= songMilliseconds) {
        return {};
    }

    const long long position = loopEnabled
        ? elapsedMilliseconds % songMilliseconds
        : elapsedMilliseconds;
    const int cycle = static_cast<int>(elapsedMilliseconds / songMilliseconds);
    return {static_cast<int>(position / beatMilliseconds), cycle,
            loopEnabled && cycle > 0 && position == 0};
}
