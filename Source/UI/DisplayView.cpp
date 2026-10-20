#include "DisplayView.h"
#include "Lyrics/LrcParser.h"

DisplayView::DisplayView(VocLyricsAudioProcessor& p) : processor(p)
{
    style.addItem("Single line", 1);
    style.addItem("Scrolling", 2);
    style.addItem("Teleprompter", 3);
    style.setSelectedId(1);
    addAndMakeVisible(style);
    addAndMakeVisible(proportional);
    addAndMakeVisible(clock);
    startTimerHz(15);
}
void DisplayView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff090c11));
    g.setColour(juce::Colour(0xff7f8b9a));
    g.setFont(juce::FontOptions(18.0f));
    g.drawText(titleText, 40, 30, getWidth() - 80, 36, juce::Justification::centred);
    g.setColour(juce::Colour(0xfff5f7fa));
    const auto fontSize = proportional.getToggleState() ? juce::jlimit(34.0f, 90.0f, getWidth() * 0.065f) : 58.0f;
    g.setFont(juce::FontOptions(fontSize, juce::Font::bold));
    g.drawFittedText(displayText, getLocalBounds().reduced(70, 96), juce::Justification::centred, 3);
    if (clock.getToggleState())
    {
        g.setColour(juce::Colour(0xff9aa4b2));
        g.setFont(juce::FontOptions(18.0f));
        g.drawText(clockText, getWidth() - 120, 20, 95, 28, juce::Justification::centredRight);
    }
}
void DisplayView::resized()
{
    auto top = getLocalBounds().removeFromTop(38).reduced(8, 4);
    style.setBounds(top.removeFromLeft(145));
    proportional.setBounds(top.removeFromLeft(150));
    clock.setBounds(top.removeFromLeft(90));
}
void DisplayView::timerCallback()
{
    const auto model = processor.getSetlistSnapshot();
    const auto* song = model.activeSong();
    titleText = song != nullptr ? song->title : "VocLyrics";
    displayText.clear();
    if (song != nullptr)
    {
        auto position = processor.getTransportSnapshot().positionSeconds;
        if (song->startSeconds >= 0.0) position -= song->startSeconds;
        auto active = voclyrics::activeLineAt(*song, position);
        if (active < 0) active = 0;
        const auto mode = style.getSelectedId();
        if (mode == 1)
        {
            for (int i = active; i >= 0; --i)
                if (! song->lyrics[static_cast<std::size_t>(i)].isSection)
                { displayText = song->lyrics[static_cast<std::size_t>(i)].text; break; }
        }
        else
        {
            const auto start = mode == 3 ? juce::jmax(0, active - 2) : juce::jmax(0, active - 1);
            for (int i = start; i < static_cast<int>(song->lyrics.size()) && i < start + (mode == 3 ? 8 : 5); ++i)
            {
                const auto& line = song->lyrics[static_cast<std::size_t>(i)];
                if (line.isSection) continue;
                if (displayText.isNotEmpty()) displayText += "\n";
                displayText += line.text;
            }
        }
    }
    clockText = juce::Time::getCurrentTime().formatted("%H:%M");
    repaint();
}
