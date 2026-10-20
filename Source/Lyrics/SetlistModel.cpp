#include "SetlistModel.h"

#include <algorithm>
#include <utility>

namespace voclyrics
{
SetlistModel::SetlistModel() { addSong(); }
Song* SetlistModel::activeSong() noexcept { return active >= 0 && active < static_cast<int>(songList.size()) ? &songList[static_cast<std::size_t>(active)] : nullptr; }
const Song* SetlistModel::activeSong() const noexcept { return active >= 0 && active < static_cast<int>(songList.size()) ? &songList[static_cast<std::size_t>(active)] : nullptr; }
int SetlistModel::addSong(std::string title)
{
    Song song;
    song.id = "song-" + std::to_string(nextId++);
    song.title = std::move(title);
    songList.push_back(std::move(song));
    active = static_cast<int>(songList.size()) - 1;
    return active;
}
bool SetlistModel::duplicateSong(int index)
{
    if (index < 0 || index >= static_cast<int>(songList.size())) return false;
    auto copy = songList[static_cast<std::size_t>(index)];
    copy.id = "song-" + std::to_string(nextId++);
    copy.title += " copy";
    songList.insert(songList.begin() + index + 1, std::move(copy));
    active = index + 1;
    return true;
}
bool SetlistModel::removeSong(int index)
{
    if (songList.size() <= 1 || index < 0 || index >= static_cast<int>(songList.size())) return false;
    songList.erase(songList.begin() + index);
    if (active >= static_cast<int>(songList.size())) active = static_cast<int>(songList.size()) - 1;
    else if (active > index) --active;
    return true;
}
bool SetlistModel::moveSong(int from, int to)
{
    if (from < 0 || to < 0 || from >= static_cast<int>(songList.size()) || to >= static_cast<int>(songList.size())) return false;
    auto item = std::move(songList[static_cast<std::size_t>(from)]);
    songList.erase(songList.begin() + from);
    songList.insert(songList.begin() + to, std::move(item));
    if (active == from) active = to;
    else if (from < active && to >= active) --active;
    else if (from > active && to <= active) ++active;
    return true;
}
bool SetlistModel::setActive(int index)
{
    if (index < 0 || index >= static_cast<int>(songList.size())) return false;
    active = index;
    return true;
}
bool SetlistModel::skip(int direction)
{
    const auto next = active + (direction < 0 ? -1 : 1);
    return setActive(std::clamp(next, 0, static_cast<int>(songList.size()) - 1));
}
bool SetlistModel::renameSong(int index, const std::string& title)
{
    if (index < 0 || index >= static_cast<int>(songList.size())) return false;
    songList[static_cast<std::size_t>(index)].title = title;
    return true;
}
bool SetlistModel::setStartSeconds(int index, double seconds)
{
    if (index < 0 || index >= static_cast<int>(songList.size()) || seconds < 0.0) return false;
    songList[static_cast<std::size_t>(index)].startSeconds = seconds;
    return true;
}
bool SetlistModel::setAutoAdvance(int index, bool enabled)
{
    if (index < 0 || index >= static_cast<int>(songList.size())) return false;
    songList[static_cast<std::size_t>(index)].autoAdvance = enabled;
    return true;
}
void SetlistModel::replace(std::vector<Song> songs, int activeIndex)
{
    songList = std::move(songs);
    if (songList.empty()) addSong();
    nextId = 1;
    for (const auto& song : songList)
    {
        constexpr auto prefix = "song-";
        if (song.id.compare(0, sizeof(prefix) - 1, prefix) != 0) continue;
        try
        {
            const auto suffix = static_cast<unsigned>(std::stoul(song.id.substr(sizeof(prefix) - 1)));
            if (suffix < static_cast<unsigned>(-1)) nextId = std::max(nextId, suffix + 1);
        }
        catch (...) {}
    }
    active = std::clamp(activeIndex, 0, static_cast<int>(songList.size()) - 1);
}
}
