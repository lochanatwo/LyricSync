#pragma once

#include <array>
#include <cmath>
#include <cstddef>

namespace voclyrics
{
class TapTempo
{
public:
    void reset() noexcept
    {
        lastTapMilliseconds = -1.0;
        count = 0;
        taps = 0;
        intervals.fill(0.0);
    }

    int tap(double nowMilliseconds) noexcept
    {
        if (lastTapMilliseconds >= 0.0)
        {
            const auto interval = nowMilliseconds - lastTapMilliseconds;
            if (interval < 120.0) return bpm();
            if (interval > 8000.0)
            {
                reset();
                lastTapMilliseconds = nowMilliseconds;
                taps = 1;
                return 0;
            }
            if (count < static_cast<int>(intervals.size()))
                intervals[static_cast<std::size_t>(count++)] = interval;
            else
            {
                for (std::size_t i = 1; i < intervals.size(); ++i)
                    intervals[i - 1] = intervals[i];
                intervals.back() = interval;
            }
        }
        lastTapMilliseconds = nowMilliseconds;
        ++taps;
        return bpm();
    }

    int bpm() const noexcept
    {
        if (taps < 4 || count == 0) return 0;
        double total = 0.0;
        for (int i = 0; i < count; ++i)
            total += intervals[static_cast<std::size_t>(i)];
        return static_cast<int>(std::lround(60000.0 * count / total));
    }

    int tapCount() const noexcept { return taps; }

private:
    std::array<double, 5> intervals {};
    double lastTapMilliseconds = -1.0;
    int count = 0;
    int taps = 0;
};
}
