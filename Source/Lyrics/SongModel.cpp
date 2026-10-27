#include "SongModel.h"

#include <cctype>
#include <string>

namespace voclyrics
{
int keyPitchClass(const std::string& text)
{
    std::size_t position = 0;
    if (text.compare(0, 3, "\xef\xbb\xbf") == 0) position = 3;
    const auto skipSpace = [&]
    {
        while (position < text.size() && std::isspace(static_cast<unsigned char>(text[position]))) ++position;
    };
    skipSpace();
    if (position + 3 > text.size()
        || std::tolower(static_cast<unsigned char>(text[position])) != 'k'
        || std::tolower(static_cast<unsigned char>(text[position + 1])) != 'e'
        || std::tolower(static_cast<unsigned char>(text[position + 2])) != 'y')
        return -1;
    position += 3;
    skipSpace();
    if (position >= text.size() || (text[position] != '-' && text[position] != ':')) return -1;
    ++position;
    skipSpace();
    if (position >= text.size()) return -1;
    const auto root = static_cast<char>(std::toupper(static_cast<unsigned char>(text[position++])));
    if (root < 'A' || root > 'G') return -1;
    char accidental = '\0';
    if (position < text.size() && (text[position] == '#' || text[position] == 'b'))
        accidental = text[position++];
    else if (text.compare(position, 3, "\xe2\x99\xaf") == 0)
    {
        accidental = '#';
        position += 3;
    }
    else if (text.compare(position, 3, "\xe2\x99\xad") == 0)
    {
        accidental = 'b';
        position += 3;
    }
    skipSpace();
    if (position != text.size())
    {
        if (text[position] == '|' || text[position] == '(')
        {
            ++position;
            skipSpace();
        }
        constexpr const char* label = "transposed by:";
        for (std::size_t i = 0; label[i] != '\0'; ++i, ++position)
            if (position >= text.size()
                || std::tolower(static_cast<unsigned char>(text[position])) != label[i])
                return -1;
        skipSpace();
        if (position < text.size() && (text[position] == '+' || text[position] == '-')) ++position;
        const auto digitsStart = position;
        while (position < text.size() && std::isdigit(static_cast<unsigned char>(text[position]))) ++position;
        if (digitsStart == position) return -1;
        skipSpace();
        if (position < text.size() && text[position] == ')') { ++position; skipSpace(); }
        if (position != text.size()) return -1;
    }
    static constexpr int naturalPitches[] { 9, 11, 0, 2, 4, 5, 7 };
    auto pitch = naturalPitches[root - 'A'];
    if (accidental == '#') ++pitch;
    else if (accidental == 'b') --pitch;
    return (pitch + 12) % 12;
}

int songKeyPitchClass(const Song& song)
{
    if (song.sourceKeyOverride >= 0 && song.sourceKeyOverride < 12)
        return song.sourceKeyOverride;
    for (const auto& line : song.lyrics)
    {
        const auto pitch = keyPitchClass(line.text);
        if (pitch >= 0) return pitch;
    }
    return -1;
}

bool isSongKeyLine(const std::string& text) { return keyPitchClass(text) >= 0; }

std::string keyNameForPitchClass(int pitchClass)
{
    static constexpr const char* names[] { "C", "C#", "D", "D#", "E", "F",
                                            "F#", "G", "G#", "A", "A#", "B" };
    if (pitchClass < 0) return {};
    return names[pitchClass % 12];
}

int transposeSemitonesForKeys(int sourcePitchClass, int targetPitchClass)
{
    if (sourcePitchClass < 0 || targetPitchClass < 0) return 0;
    auto interval = (targetPitchClass - sourcePitchClass + 12) % 12;
    if (interval > 6) interval -= 12;
    return interval;
}

int activeLineAt(const Song& song, double seconds)
{
    int active = -1;
    double bestTime = -1.0;
    for (std::size_t i = 0; i < song.lyrics.size(); ++i)
        if (! song.lyrics[i].isSection && ! isSongKeyLine(song.lyrics[i].text) && song.lyrics[i].timeSeconds >= 0.0
            && song.lyrics[i].timeSeconds <= seconds && song.lyrics[i].timeSeconds >= bestTime)
        {
            active = static_cast<int>(i);
            bestTime = song.lyrics[i].timeSeconds;
        }
    return active;
}
}
