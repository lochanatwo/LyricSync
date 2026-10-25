#include "LiveTagger.h"

namespace voclyrics
{
int LiveTagger::nextUntimedLine() const
{
    for (std::size_t i = 0; i < target.lyrics.size(); ++i)
        if (! target.lyrics[i].isSection && ! isSongKeyLine(target.lyrics[i].text)
            && target.lyrics[i].timeSeconds < 0.0)
            return static_cast<int>(i);
    return -1;
}
bool LiveTagger::tagNext(double playheadSeconds)
{
    const auto index = nextUntimedLine();
    return index >= 0 && retag(index, playheadSeconds);
}
bool LiveTagger::retag(int lineIndex, double playheadSeconds)
{
    if (lineIndex < 0 || lineIndex >= static_cast<int>(target.lyrics.size()) || playheadSeconds < 0.0)
        return false;
    lastTagged = lineIndex;
    priorTime = target.lyrics[static_cast<std::size_t>(lineIndex)].timeSeconds;
    target.lyrics[static_cast<std::size_t>(lineIndex)].timeSeconds = playheadSeconds;
    return true;
}
bool LiveTagger::undo()
{
    if (lastTagged < 0 || lastTagged >= static_cast<int>(target.lyrics.size())) return false;
    target.lyrics[static_cast<std::size_t>(lastTagged)].timeSeconds = priorTime;
    lastTagged = -1;
    priorTime = -1.0;
    return true;
}
}
