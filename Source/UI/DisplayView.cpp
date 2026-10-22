#include "DisplayView.h"
#include "Lyrics/LrcParser.h"
#include <cctype>
#include <sstream>
#include <utility>
#include <vector>

namespace
{
bool isChordOnlyLine(const std::string& text)
{
    std::istringstream input(text);
    std::string token;
    bool found = false;
    while (input >> token)
    {
        found = true;
        if (token.empty() || token.front() < 'A' || token.front() > 'G') return false;
        for (std::size_t i = 1; i < token.size(); ++i)
        {
            const auto c = token[i];
            if (! std::isalnum(static_cast<unsigned char>(c)) && c != '#' && c != 'b' && c != '/'
                && c != '+' && c != '-' && c != '(' && c != ')') return false;
        }
    }
    return found;
}

struct DisplayBlock
{
    int sourceIndex = 0;
    juce::String chords;
    juce::String lyrics;
};
}

DisplayView::DisplayView(VocLyricsAudioProcessor& p) : processor(p)
{
    addAndMakeVisible(proportional);
    addAndMakeVisible(clock);
    addAndMakeVisible(upButton);
    addAndMakeVisible(downButton);
    upButton.onClick = [this] { processor.moveSharedScrollLine(-1); };
    downButton.onClick = [this] { processor.moveSharedScrollLine(1); };
    for (auto* button : { &upButton, &downButton })
    {
        button->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1b2937));
        button->setColour(juce::TextButton::textColourOffId, juce::Colour(0xff57e0b5));
    }
    startTimerHz(30);
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

    auto cards = getLocalBounds().reduced(42, 112);
    cards.removeFromRight(78);
    auto now = cards.removeFromTop(static_cast<int>(cards.getHeight() * 0.68f));
    g.setColour(juce::Colour(0xdd141b28));
    g.fillRoundedRectangle(now.toFloat(), 24.0f);
    g.setColour(juce::Colour(0xff57e0b5));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("NOW", now.reduced(28).removeFromTop(24), juce::Justification::centredLeft);
    auto currentArea = now.reduced(34, 42);
    currentArea.removeFromTop(16);
    const auto rowHeight = currentArea.getHeight() / 2;
    const auto lyricSize = proportional.getToggleState() ? juce::jlimit(28.0f, 60.0f, getWidth() * 0.046f) : 43.0f;
    for (std::size_t i = 0; i < currentLyrics.size(); ++i)
    {
        auto row = currentArea.removeFromTop(rowHeight);
        auto chordArea = row.removeFromTop(juce::jlimit(24, 42, row.getHeight() / 3));
        g.setColour(juce::Colour(0xffff8a45));
        g.setFont(juce::FontOptions(juce::jlimit(18.0f, 28.0f, lyricSize * 0.47f), juce::Font::bold));
        g.drawFittedText(currentChords[i], chordArea, juce::Justification::centredLeft, 1);
        g.setColour(i == 0 ? juce::Colour(0xff57e0b5) : juce::Colour(0xfff8fafc));
        g.setFont(juce::FontOptions(lyricSize, juce::Font::bold));
        g.drawFittedText(currentLyrics[i], row, juce::Justification::centredLeft, 2);
    }

    cards.removeFromTop(18);
    g.setColour(juce::Colour(0xaa111722));
    g.fillRoundedRectangle(cards.toFloat(), 20.0f);
    g.setColour(juce::Colour(0xff79869a));
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText("UP NEXT", cards.reduced(24).removeFromTop(20), juce::Justification::centredLeft);
    auto upcomingArea = cards.reduced(28, 30);
    upcomingArea.removeFromTop(12);
    const auto upcomingRowHeight = upcomingArea.getHeight() / 2;
    for (std::size_t i = 0; i < upcomingLyrics.size(); ++i)
    {
        auto row = upcomingArea.removeFromTop(upcomingRowHeight);
        auto chordArea = row.removeFromTop(18);
        g.setColour(juce::Colour(0xffd97943));
        g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        g.drawFittedText(upcomingChords[i], chordArea, juce::Justification::centredLeft, 1);
        g.setColour(juce::Colour(0xffaeb9c9));
        g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
        g.drawFittedText(upcomingLyrics[i], row, juce::Justification::centredLeft, 1);
    }
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
    auto controls = getLocalBounds().removeFromRight(82).withSizeKeepingCentre(62, 154);
    upButton.setBounds(controls.removeFromTop(70));
    controls.removeFromTop(14);
    downButton.setBounds(controls.removeFromTop(70));
}
void DisplayView::timerCallback()
{
    const auto model = processor.getSetlistSnapshot();
    const auto* song = model.activeSong();
    titleText = song != nullptr ? song->title : "VocLyrics";
    transposeText = juce::String("TRANSPOSED BY: ") + juce::String(processor.getTransposeSemitones());
    roleText = processor.getHostRole().toUpperCase();
    for (auto* collection : { &currentChords, &currentLyrics, &upcomingChords, &upcomingLyrics })
        for (auto& text : *collection) text.clear();
    if (song != nullptr && ! song->lyrics.empty())
    {
        auto position = processor.getTransportSnapshot().positionSeconds;
        if (song->startSeconds >= 0.0) position -= song->startSeconds;
        auto active = processor.getSharedScrollLine();
        if (active < 0) active = voclyrics::activeLineAt(*song, position);
        if (active < 0) active = 0;
        active = juce::jlimit(0, static_cast<int>(song->lyrics.size()) - 1, active);
        std::vector<DisplayBlock> blocks;
        for (int i = 0; i < static_cast<int>(song->lyrics.size()); ++i)
        {
            const auto& line = song->lyrics[static_cast<std::size_t>(i)];
            if (line.isSection) continue;
            DisplayBlock block;
            block.sourceIndex = i;
            if (isChordOnlyLine(line.text) && i + 1 < static_cast<int>(song->lyrics.size())
                && ! song->lyrics[static_cast<std::size_t>(i + 1)].isSection)
            {
                block.chords = juce::String(line.text);
                block.lyrics = juce::String(song->lyrics[static_cast<std::size_t>(++i)].text);
            }
            else
            {
                block.lyrics = juce::String(line.text);
            }
            blocks.push_back(std::move(block));
        }
        std::size_t start = 0;
        for (std::size_t i = 0; i < blocks.size(); ++i)
            if (blocks[i].sourceIndex <= active) start = i;
        for (std::size_t i = 0; i < 2 && start + i < blocks.size(); ++i)
        {
            currentChords[i] = blocks[start + i].chords;
            currentLyrics[i] = blocks[start + i].lyrics;
        }
        for (std::size_t i = 0; i < 2 && start + i + 2 < blocks.size(); ++i)
        {
            upcomingChords[i] = blocks[start + i + 2].chords;
            upcomingLyrics[i] = blocks[start + i + 2].lyrics;
        }
    }
    clockText = juce::Time::getCurrentTime().formatted("%H:%M");
    const auto hostControls = processor.hostControlsSharedScroll();
    upButton.setEnabled(hostControls);
    downButton.setEnabled(hostControls);
    repaint();
}
