#pragma once

#include <JuceHeader.h>

namespace voclyrics
{
class QrCode
{
public:
    static void draw(juce::Graphics& graphics, juce::Rectangle<float> bounds, const juce::String& text);
};
}
