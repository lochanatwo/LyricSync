#pragma once

#include "PluginProcessor.h"
#include <JuceHeader.h>
#include <functional>
#include <vector>

class SetlistPanel final : public juce::Component
{
public:
    explicit SetlistPanel(VocLyricsAudioProcessor& processor);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    void refresh();
    void focusSearch();
    std::function<void()> onSelectionChanged;

private:
    VocLyricsAudioProcessor& processor;
    juce::TextEditor search;
    juce::TextButton add { "+ Song" }, duplicate { "Duplicate" }, remove { "Delete" };
    juce::TextButton up { "↑" }, down { "↓" }, previous { "‹" }, next { "›" };
    int pressedRow = -1;
    bool dragged = false;
    std::vector<int> matchingSongIndices() const;
};
