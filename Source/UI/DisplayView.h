#pragma once

#include "PluginProcessor.h"
#include "ShortcutTextEditor.h"
#include <JuceHeader.h>
#include <array>
#include <functional>
#include <utility>
#include <vector>

class DisplayView final : public juce::Component, private juce::Timer
{
public:
    explicit DisplayView(VocLyricsAudioProcessor& processor);
    void paint(juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;
    void focusSongSearch();
    void setShortcutHandler(std::function<bool(const juce::KeyPress&)> handler)
    {
        songSearch.onShortcut = std::move(handler);
    }
private:
    void timerCallback() override;
    void refreshSongMatches();
    VocLyricsAudioProcessor& processor;
    juce::ToggleButton proportional { "Proportional text" }, clock { "Clock" }, autoScroll { "Auto scroll" };
    juce::TextButton tapTempoButton { "Tap tempo" }, stopTempoButton { "Stop" };
    juce::TextButton useManualBpmButton { "Use BPM" };
    juce::Label tapTempoStatus;
    juce::Slider manualTempoBpm;
    juce::Slider autoScrollSpeed;
    juce::ComboBox sourceKeyChoice;
    juce::ComboBox targetKeyChoice;
    ShortcutTextEditor songSearch;
    juce::ComboBox songResults;
    juce::TextButton upButton { juce::String::charToString(0x2191) };
    juce::TextButton downButton { juce::String::charToString(0x2193) };
    std::array<juce::String, 25> visibleChords, visibleLyrics;
    std::array<juce::String, 25> previousChords, previousLyrics;
    std::vector<int> matchingSongIndices;
    juce::String titleText, clockText, transposeText, roleText, sectionText, keyText, displayedSongId;
    int displaySourceIndex = -1;
    int animationDirection = 1;
    int animationRows = 1;
    double animationStartMilliseconds = 0.0;
};
