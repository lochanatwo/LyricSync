#pragma once

#include <cctype>
#include <sstream>
#include <string>

namespace voclyrics
{
inline bool isChordOnlyLine(const std::string& text)
{
    std::istringstream input(text);
    std::string token;
    bool found = false;
    while (input >> token)
    {
        while (! token.empty() && (token.front() == '|' || token.front() == ':')) token.erase(token.begin());
        while (! token.empty() && (token.back() == '|' || token.back() == ':')) token.pop_back();
        if (token.empty()) continue;
        found = true;
        if (token.front() < 'A' || token.front() > 'G') return false;
        for (std::size_t i = 1; i < token.size(); ++i)
        {
            const auto c = token[i];
            if (! std::isalnum(static_cast<unsigned char>(c)) && c != '#' && c != 'b' && c != '/'
                && c != '+' && c != '-' && c != '(' && c != ')') return false;
        }
    }
    return found;
}

inline std::string transposeChordLine(const std::string& source, int semitones)
{
    static constexpr int naturalPitches[] { 9, 11, 0, 2, 4, 5, 7 };
    static constexpr const char* sharpNames[] { "C", "C#", "D", "D#", "E", "F",
                                                "F#", "G", "G#", "A", "A#", "B" };
    const auto transposeRoot = [&](char root, char accidental)
    {
        auto pitch = naturalPitches[root - 'A'] + semitones;
        if (accidental == '#') ++pitch;
        else if (accidental == 'b') --pitch;
        return std::string(sharpNames[(pitch % 12 + 12) % 12]);
    };
    std::string output;
    for (std::size_t position = 0; position < source.size();)
    {
        if (std::isspace(static_cast<unsigned char>(source[position])))
        {
            output += source[position++];
            continue;
        }
        const auto begin = position;
        while (position < source.size() && ! std::isspace(static_cast<unsigned char>(source[position]))) ++position;
        const auto token = source.substr(begin, position - begin);
        if (semitones == 0) { output += token; continue; }
        std::string changed;
        std::size_t cursor = 0;
        while (cursor < token.size() && (token[cursor] == '|' || token[cursor] == ':'))
            changed += token[cursor++];
        if (cursor >= token.size() || token[cursor] < 'A' || token[cursor] > 'G')
        {
            output += token;
            continue;
        }
        const auto root = token[cursor++];
        const auto accidental = cursor < token.size() && (token[cursor] == '#' || token[cursor] == 'b')
            ? token[cursor++] : '\0';
        changed += transposeRoot(root, accidental);
        while (cursor < token.size())
        {
            if (token[cursor] == '/' && cursor + 1 < token.size()
                && token[cursor + 1] >= 'A' && token[cursor + 1] <= 'G')
            {
                changed += '/';
                const auto bassRoot = token[++cursor];
                ++cursor;
                const auto bassAccidental = cursor < token.size()
                    && (token[cursor] == '#' || token[cursor] == 'b') ? token[cursor++] : '\0';
                changed += transposeRoot(bassRoot, bassAccidental);
            }
            else changed += token[cursor++];
        }
        output += changed;
    }
    return output;
}
}
