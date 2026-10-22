#include "DisplayView.h"
#include "Lyrics/LrcParser.h"
#include <vector>

DisplayView::DisplayView(VocLyricsAudioProcessor& p) : processor(p)
{
    addAndMakeVisible(proportional);
    addAndMakeVisible(clock);
    startTimerHz(15);
}
void DisplayView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff080b12));
    juce::ColourGradient glow(juce::Colour(0x4439d9b0), getWidth() * 0.5f, 0.0f,
                              juce::Colour(0x00101828), getWidth() * 0.5f, getHeight() * 0.65f, false);
    g.setGradientFill(glow);
    g.fillRect(getLocalBounds());

    g.setColour(juce::Colour(0xff92a0b5));
    g.setFont(juce::FontOptions(16.0f, juce::Font::bold));
    g.drawText(roleText + "  •  " + transposeText, 40, 46, getWidth() - 80, 24, juce::Justification::centred);
    g.setColour(juce::Colour(0xfff5f7fa));
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText(titleText, 40, 72, getWidth() - 80, 34, juce::Justification::centred);

    auto cards = getLocalBounds().reduced(70, 122);
    auto now = cards.removeFromTop(static_cast<int>(cards.getHeight() * 0.64f));
    g.setColour(juce::Colour(0xdd141b28));
    g.fillRoundedRectangle(now.toFloat(), 24.0f);
    g.setColour(juce::Colour(0xff57e0b5));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("NOW", now.reduced(28).removeFromTop(24), juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xfff8fafc));
    const auto fontSize = proportional.getToggleState() ? juce::jlimit(32.0f, 72.0f, getWidth() * 0.054f) : 48.0f;
    g.setFont(juce::FontOptions(fontSize, juce::Font::bold));
    g.drawFittedText(currentText, now.reduced(34, 44), juce::Justification::centred, 2);

    cards.removeFromTop(18);
    g.setColour(juce::Colour(0xaa111722));
    g.fillRoundedRectangle(cards.toFloat(), 20.0f);
    g.setColour(juce::Colour(0xff79869a));
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText("UP NEXT", cards.reduced(24).removeFromTop(20), juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xffb8c1cf));
    g.setFont(juce::FontOptions(26.0f, juce::Font::bold));
    g.drawFittedText(upcomingText, cards.reduced(28, 32), juce::Justification::centred, 2);
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
    proportional.setBounds(top.removeFromLeft(150));
    clock.setBounds(top.removeFromLeft(90));
}
void DisplayView::timerCallback()
{
    const auto model = processor.getSetlistSnapshot();
    const auto* song = model.activeSong();
    titleText = song != nullptr ? song->title : "VocLyrics";
    transposeText = juce::String("TRANSPOSED BY: ") + juce::String(processor.getTransposeSemitones());
    roleText = processor.getHostRole().toUpperCase();
    currentText.clear();
    upcomingText.clear();
    if (song != nullptr && ! song->lyrics.empty())
    {
        auto position = processor.getTransportSnapshot().positionSeconds;
        if (song->startSeconds >= 0.0) position -= song->startSeconds;
        auto active = processor.getSharedScrollLine();
        if (active < 0) active = voclyrics::activeLineAt(*song, position);
        if (active < 0) active = 0;
        active = juce::jlimit(0, static_cast<int>(song->lyrics.size()) - 1, active);
        std::vector<juce::String> visible;
        for (int i = active; i < static_cast<int>(song->lyrics.size()) && visible.size() < 4; ++i)
        {
            const auto& line = song->lyrics[static_cast<std::size_t>(i)];
            if (! line.isSection) visible.emplace_back(line.text);
        }
        for (std::size_t i = 0; i < visible.size() && i < 2; ++i)
        {
            if (currentText.isNotEmpty()) currentText += "\n";
            currentText += visible[i];
        }
        for (std::size_t i = 2; i < visible.size(); ++i)
        {
            if (upcomingText.isNotEmpty()) upcomingText += "\n";
            upcomingText += visible[i];
        }
    }
    clockText = juce::Time::getCurrentTime().formatted("%H:%M");
    repaint();
}
