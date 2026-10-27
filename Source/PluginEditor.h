#pragma once

#include "PluginProcessor.h"
#include "UI/DisplayView.h"
#include "UI/EditorPanel.h"
#include "UI/LookAndFeel.h"
#include "UI/RemotePanel.h"
#include "UI/SetlistPanel.h"

class VocLyricsAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                           private juce::Timer
{
public:
    explicit VocLyricsAudioProcessorEditor(VocLyricsAudioProcessor&);
    ~VocLyricsAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;
private:
    void timerCallback() override;
    void reloadSong();
    VocLyricsAudioProcessor& audioProcessor;
    voclyrics::LookAndFeel lookAndFeel;
    SetlistPanel setlist;
    EditorPanel editor;
    RemotePanel remote;
    DisplayView display;
    juce::TextButton displayToggle { "Performance view" };
    bool displayMode = false;
    bool manualDisplayMode = false;
    bool tapShortcutHeld = false;
};
