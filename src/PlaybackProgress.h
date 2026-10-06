#pragma once

class PlaybackProgress
{
public:
    struct State {
        int step = -1;
        int cycle = 0;
        bool atLoopStart = false;
    };

    [[nodiscard]] static State stateAt(long long elapsedMilliseconds,
                                       int totalSteps,
                                       int beatMilliseconds,
                                       bool loopEnabled) noexcept;
};
