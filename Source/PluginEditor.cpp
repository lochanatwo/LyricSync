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
    display.setVisible(false);
    setlist.onSelectionChanged = [this] { reloadSong(); };
    startTimerHz(20);
}

VocLyricsAudioProcessorEditor::~VocLyricsAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void VocLyricsAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff11151c));
    g.setColour(juce::Colour(0xff2a3442));
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
}

bool VocLyricsAudioProcessorEditor::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress(juce::KeyPress::returnKey, juce::ModifierKeys::commandModifier, 0)
        || key == juce::KeyPress(juce::KeyPress::returnKey, juce::ModifierKeys::ctrlModifier, 0))
    {
        audioProcessor.tagNextLine();
        editor.loadFromProcessor();
        return true;
    }
    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0)
        || key == juce::KeyPress('z', juce::ModifierKeys::ctrlModifier, 0))
    {
        audioProcessor.undoTag();
        editor.loadFromProcessor();
        return true;
    }
    return false;
}

void VocLyricsAudioProcessorEditor::timerCallback()
{
    const auto playing = audioProcessor.getTransportSnapshot().isPlaying;
    if (playing != displayMode)
    {
        displayMode = playing;
        setlist.setVisible(! displayMode);
        editor.setVisible(! displayMode);
        remote.setVisible(! displayMode);
        display.setVisible(displayMode);
        resized();
    }
    setlist.refresh();
}

void VocLyricsAudioProcessorEditor::reloadSong()
{
    editor.loadFromProcessor();
    setlist.refresh();
}
