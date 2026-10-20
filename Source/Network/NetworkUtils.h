#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace voclyrics
{
class NetworkUtils
{
public:
    static std::vector<juce::String> localIPv4Addresses();
    static juce::String preferredIPv4Address();
};
}
