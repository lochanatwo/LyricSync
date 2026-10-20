#include "NetworkUtils.h"
#include <algorithm>

namespace voclyrics
{
std::vector<juce::String> NetworkUtils::localIPv4Addresses()
{
    std::vector<juce::String> result;
    for (const auto& address : juce::IPAddress::getAllAddresses(false))
    {
        const auto value = address.toString();
        if (value.startsWith("127.") || value.startsWith("169.254.") || value.startsWith("0.")
            || value.startsWith("224.") || value.startsWith("172.17.") || value.startsWith("172.18.")
            || value.startsWith("192.168.56.") || value.startsWith("192.168.99.") || value.startsWith("10.0.75."))
            continue;
        if (std::find(result.begin(), result.end(), value) == result.end()) result.push_back(value);
    }
    const auto score = [](const juce::String& address)
    {
        if (address.startsWith("192.168.")) return 0;
        if (address.startsWith("10.")) return 1;
        if (address.startsWith("172.")) return 2;
        return 3;
    };
    std::stable_sort(result.begin(), result.end(), [&](const auto& a, const auto& b) { return score(a) < score(b); });
    return result;
}

juce::String NetworkUtils::preferredIPv4Address()
{
    const auto addresses = localIPv4Addresses();
    return addresses.empty() ? juce::String("127.0.0.1") : addresses.front();
}
}
