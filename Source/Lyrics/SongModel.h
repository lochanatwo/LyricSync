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
    double startSeconds = -1.0;
    bool autoAdvance = false;
};

int activeLineAt(const Song& song, double seconds);
}
