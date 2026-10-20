#pragma once

#include "PluginProcessor.h"
#include <JuceHeader.h>

class EditorPanel final : public juce::Component, private juce::Timer
{
public:
    explicit EditorPanel(VocLyricsAudioProcessor& processor);
    void resized() override;
    void loadFromProcessor();
    void timerCallback() override;
private:
    VocLyricsAudioProcessor& processor;
    juce::TextEditor lyrics;
    juce::TextButton tag { "Tag next line" }, undo { "Undo tag" }, import { "Import .lrc" }, exportFile { "Export .lrc" };
    juce::TextButton previousSection { "Prev section" }, nextSection { "Next section" };
    juce::TextButton previousLine { "Shared line −" }, nextLine { "Shared line +" };
    juce::ToggleButton shareScroll { "Share scroll" };
    std::unique_ptr<juce::FileChooser> chooser;
    void moveSection(int direction);
};
