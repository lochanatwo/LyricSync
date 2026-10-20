#pragma once

#include <string>
#include <vector>

namespace voclyrics
{
struct LyricLine
{
    double timeSeconds = -1.0;
    std::string text;
    bool isSection = false;
};

class LrcParser
{
public:
    static std::vector<LyricLine> parse(const std::string& source);
    static std::string serialize(const std::vector<LyricLine>& lines);
};
}
