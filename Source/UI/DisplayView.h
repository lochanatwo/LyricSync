#pragma once

#include "PluginProcessor.h"
#include <JuceHeader.h>

class DisplayView final : public juce::Component, private juce::Timer
{
public:
    explicit DisplayView(VocLyricsAudioProcessor& processor);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    VocLyricsAudioProcessor& processor;
    juce::ComboBox style;
    juce::ToggleButton proportional { "Proportional text" }, clock { "Clock" };
    juce::String titleText, displayText, clockText;
};
