#pragma once

namespace voclyrics
{
struct SyncFrame
{
    double positionSeconds = 0.0;
    double hostTimeMilliseconds = 0.0;
    bool playing = false;
};

class SyncClock
{
public:
    static double estimatePosition(const SyncFrame& frame,
                                   double clientNowMilliseconds,
                                   double hostOffsetMilliseconds,
                                   double nudgeMilliseconds) noexcept;
};
}
