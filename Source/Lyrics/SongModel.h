#pragma once

#include "LrcParser.h"
#include <string>
#include <vector>

namespace voclyrics
{
struct Song
{
    std::string id;
    std::string title = "Untitled Song";
    std::vector<LyricLine> lyrics;
    int sourceKeyOverride = -1;
    double startSeconds = -1.0;
    bool autoAdvance = false;
};

int activeLineAt(const Song& song, double seconds);
int keyPitchClass(const std::string& text);
int songKeyPitchClass(const Song& song);
bool isSongKeyLine(const std::string& text);
std::string keyNameForPitchClass(int pitchClass);
int transposeSemitonesForKeys(int sourcePitchClass, int targetPitchClass);
}
