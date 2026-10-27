#pragma once

#include "PluginProcessor.h"
#include "ShortcutTextEditor.h"
#include <JuceHeader.h>
#include <functional>
#include <utility>
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
    juce::String searchQuery() const { return search.getText().trim(); }
    void setShortcutHandler(std::function<bool(const juce::KeyPress&)> handler)
    {
        search.onShortcut = std::move(handler);
    }
    std::function<void()> onSelectionChanged;

private:
    VocLyricsAudioProcessor& processor;
    ShortcutTextEditor search;
    juce::TextButton add { "+ Song" }, duplicate { "Duplicate" }, remove { "Delete" };
    juce::TextButton up { "↑" }, down { "↓" }, previous { "‹" }, next { "›" };
    int pressedRow = -1;
    bool dragged = false;
    std::vector<int> matchingSongIndices(const voclyrics::SetlistModel& model) const;
};
