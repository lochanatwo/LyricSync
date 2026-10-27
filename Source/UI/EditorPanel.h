#pragma once

#include "PluginProcessor.h"
#include "ShortcutTextEditor.h"
#include <JuceHeader.h>
#include <utility>

class EditorPanel final : public juce::Component, private juce::Timer
{
public:
    explicit EditorPanel(VocLyricsAudioProcessor& processor);
    void resized() override;
    void loadFromProcessor();
    void focusLyricsMatch(const juce::String& query);
    void setShortcutHandler(std::function<bool(const juce::KeyPress&)> handler)
    {
        lyrics.onShortcut = std::move(handler);
    }
    void timerCallback() override;
private:
    VocLyricsAudioProcessor& processor;
    ShortcutTextEditor lyrics;
    juce::TextButton import { "Import lyrics" }, exportFile { "Export lyrics" };
    juce::TextButton previousSection { "Prev section" }, nextSection { "Next section" };
    juce::TextButton previousLine { "Shared line −" }, nextLine { "Shared line +" };
    juce::ToggleButton shareScroll { "Share scroll" };
    juce::ToggleButton autoScroll { "Auto scroll" };
    juce::TextButton tapTempoButton { "Tap tempo" }, stopTempoButton { "Stop" };
    juce::TextButton useManualBpmButton { "Use BPM" };
    juce::Label tapTempoStatus;
    juce::Slider manualTempoBpm;
    juce::Label autoScrollSpeedLabel;
    juce::Slider autoScrollSpeed;
    juce::Label originalKeyStatus;
    juce::ComboBox sourceKeyChoice;
    juce::Label transposeStatus;
    juce::ComboBox transposeChoice;
    std::unique_ptr<juce::FileChooser> chooser;
    void moveSection(int direction);
    void moveCaretLine(int direction);
    void refreshKeyControls();
};
