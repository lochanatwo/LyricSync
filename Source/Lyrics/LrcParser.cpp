#include "LrcParser.h"

#include <cctype>
#include <iomanip>
#include <sstream>

namespace voclyrics
{
namespace
{
std::string trim(const std::string& value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

bool parseTimestamp(const std::string& text, std::size_t start, std::size_t end, double& seconds)
{
    const auto stamp = text.substr(start, end - start);
    const auto colon = stamp.find(':');
    if (colon == std::string::npos)
        return false;
    try
    {
        const auto minutes = std::stoi(stamp.substr(0, colon));
        const auto remainder = stamp.substr(colon + 1);
        const auto dot = remainder.find_first_of(".,");
        const auto wholeSeconds = std::stoi(remainder.substr(0, dot));
        double fraction = 0.0;
        if (dot != std::string::npos)
        {
            auto fractionText = remainder.substr(dot + 1);
            if (fractionText.size() == 1) fractionText += "00";
            else if (fractionText.size() == 2) fractionText += "0";
            else if (fractionText.size() > 3) fractionText.resize(3);
            fraction = std::stod("0." + fractionText);
        }
        if (minutes < 0 || wholeSeconds < 0 || wholeSeconds >= 60)
            return false;
        seconds = static_cast<double>(minutes * 60 + wholeSeconds) + fraction;
        return true;
    }
    catch (...)
    {
        return false;
    }
}
}

std::vector<LyricLine> LrcParser::parse(const std::string& source)
{
    std::vector<LyricLine> result;
    std::istringstream input(source);
    std::string row;
    while (std::getline(input, row))
    {
        row = trim(row);
        if (row.empty())
            continue;

        std::vector<double> stamps;
        std::size_t cursor = 0;
        while (cursor < row.size() && row[cursor] == '[')
        {
            const auto close = row.find(']', cursor + 1);
            if (close == std::string::npos)
                break;
            double time = 0.0;
            if (! parseTimestamp(row, cursor + 1, close, time))
                break;
            stamps.push_back(time);
            cursor = close + 1;
        }

        auto text = trim(row.substr(cursor));
        if (text.empty() && stamps.empty())
            continue;
        if (! text.empty() && text.front() == '[' && text.back() == ']')
        {
            result.push_back({-1.0, text.substr(1, text.size() - 2), true});
            continue;
        }
        if (stamps.empty())
            result.push_back({-1.0, text, false});
        else
            for (const auto time : stamps)
                result.push_back({time, text, false});
    }
    return result;
}

std::string LrcParser::serialize(const std::vector<LyricLine>& lines)
{
    std::ostringstream output;
    for (const auto& line : lines)
    {
        if (line.isSection)
            output << '[' << line.text << ']';
        else if (line.timeSeconds >= 0.0)
        {
            const auto totalCentiseconds = static_cast<long long>(line.timeSeconds * 100.0 + 0.5);
            output << '[' << std::setfill('0') << std::setw(2) << totalCentiseconds / 6000
                   << ':' << std::setw(2) << (totalCentiseconds / 100) % 60
                   << '.' << std::setw(2) << totalCentiseconds % 100 << ']' << line.text;
        }
        else
            output << line.text;
        output << '\n';
    }
    return output.str();
}
}
