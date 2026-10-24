#pragma once

#include "PluginProcessor.h"
#include <JuceHeader.h>
#include <array>

class DisplayView final : public juce::Component, private juce::Timer
{
public:
    explicit DisplayView(VocLyricsAudioProcessor& processor);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    VocLyricsAudioProcessor& processor;
    juce::ToggleButton proportional { "Proportional text" }, clock { "Clock" }, autoScroll { "Auto scroll" };
    juce::Slider autoScrollSpeed;
    juce::TextButton upButton { juce::String::charToString(0x2191) };
    juce::TextButton downButton { juce::String::charToString(0x2193) };
    std::array<juce::String, 5> visibleChords, visibleLyrics;
    juce::String titleText, clockText, transposeText, roleText, sectionText;
};
