#include "SyncClock.h"

#include <algorithm>

namespace voclyrics
{
double SyncClock::estimatePosition(const SyncFrame& frame,
                                  double clientNowMilliseconds,
                                  double hostOffsetMilliseconds,
                                  double nudgeMilliseconds) noexcept
{
    const auto elapsed = std::max(0.0, clientNowMilliseconds + hostOffsetMilliseconds - frame.hostTimeMilliseconds
                                        + nudgeMilliseconds);
    return frame.positionSeconds + (frame.playing ? elapsed / 1000.0 : 0.0);
}
}
