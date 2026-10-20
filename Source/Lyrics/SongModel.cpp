#include "SongModel.h"

namespace voclyrics
{
int activeLineAt(const Song& song, double seconds)
{
    int active = -1;
    double bestTime = -1.0;
    for (std::size_t i = 0; i < song.lyrics.size(); ++i)
        if (! song.lyrics[i].isSection && song.lyrics[i].timeSeconds >= 0.0
            && song.lyrics[i].timeSeconds <= seconds && song.lyrics[i].timeSeconds >= bestTime)
        {
            active = static_cast<int>(i);
            bestTime = song.lyrics[i].timeSeconds;
        }
    return active;
}
}
