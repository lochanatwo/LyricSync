#pragma once

#include "SongModel.h"

namespace voclyrics
{
class LiveTagger
{
public:
    explicit LiveTagger(Song& song) : target(song) {}
    bool tagNext(double playheadSeconds);
    bool retag(int lineIndex, double playheadSeconds);
    bool undo();
    int nextUntimedLine() const;
private:
    Song& target;
    int lastTagged = -1;
    double priorTime = -1.0;
};
}
