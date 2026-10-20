#pragma once

#include "SongModel.h"
#include <vector>

namespace voclyrics
{
class SetlistModel
{
public:
    SetlistModel();
    const std::vector<Song>& songs() const noexcept { return songList; }
    int activeIndex() const noexcept { return active; }
    Song* activeSong() noexcept;
    const Song* activeSong() const noexcept;
    int addSong(std::string title = "Untitled Song");
    bool duplicateSong(int index);
    bool removeSong(int index);
    bool moveSong(int from, int to);
    bool setActive(int index);
    bool skip(int direction);
    bool renameSong(int index, const std::string& title);
    bool setStartSeconds(int index, double seconds);
    bool setAutoAdvance(int index, bool enabled);
    void replace(std::vector<Song> songs, int activeIndex);

private:
    std::vector<Song> songList;
    int active = 0;
    unsigned nextId = 1;
};
}
