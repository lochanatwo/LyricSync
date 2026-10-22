#include "PluginEditor.h"

VocLyricsAudioProcessorEditor::VocLyricsAudioProcessorEditor(VocLyricsAudioProcessor& processor)
    : AudioProcessorEditor(&processor), audioProcessor(processor), setlist(processor),
      editor(processor), remote(processor), display(processor)
{
    setLookAndFeel(&lookAndFeel);
    setSize(1220, 760);
    addAndMakeVisible(setlist);
    addAndMakeVisible(editor);
    addAndMakeVisible(remote);
    addAndMakeVisible(display);
    addAndMakeVisible(displayToggle);
    display.setVisible(false);
    displayToggle.onClick = [this]
    {
        manualDisplayMode = ! manualDisplayMode;
        displayToggle.setButtonText(manualDisplayMode ? "Edit lyrics" : "Performance view");
        timerCallback();
    };
    setlist.onSelectionChanged = [this] { reloadSong(); };
    startTimerHz(20);
}

VocLyricsAudioProcessorEditor::~VocLyricsAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void VocLyricsAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff080b12));
    juce::ColourGradient glow(juce::Colour(0x332ed4a7), getWidth() * 0.52f, 0.0f,
                              juce::Colour(0x00080b12), getWidth() * 0.52f, getHeight() * 0.7f, false);
    g.setGradientFill(glow);
    g.fillRect(getLocalBounds());
    g.setColour(juce::Colour(0xff29364a));
    g.drawVerticalLine(setlist.getRight(), 0.0f, static_cast<float>(getHeight()));
    g.drawVerticalLine(remote.getX(), 0.0f, static_cast<float>(getHeight()));
}

void VocLyricsAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    const auto sideWidth = juce::jlimit(205, 245, getWidth() / 5);
    setlist.setBounds(bounds.removeFromLeft(sideWidth));
    remote.setBounds(bounds.removeFromRight(260));
    editor.setBounds(bounds);
    display.setBounds(getLocalBounds());
    displayToggle.setBounds(getWidth() - 172, 8, 160, 30);
    displayToggle.toFront(false);
}

bool VocLyricsAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    juce::ignoreUnused(key);
    return false;
}

void VocLyricsAudioProcessorEditor::timerCallback()
{
    const auto shouldDisplay = manualDisplayMode || audioProcessor.getTransportSnapshot().isPlaying;
    if (shouldDisplay != displayMode)
    {
        displayMode = shouldDisplay;
        setlist.setVisible(! displayMode);
        editor.setVisible(! displayMode);
        remote.setVisible(! displayMode);
        display.setVisible(displayMode);
        displayToggle.setButtonText(displayMode ? "Edit lyrics" : "Performance view");
        resized();
    }
    setlist.refresh();
}

void VocLyricsAudioProcessorEditor::reloadSong()
{
    editor.loadFromProcessor();
    setlist.refresh();
}
