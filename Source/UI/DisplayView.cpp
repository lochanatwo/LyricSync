#include "DisplayView.h"
#include "Lyrics/LrcParser.h"
#include <cmath>
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
        while (! token.empty() && (token.front() == '|' || token.front() == ':')) token.erase(token.begin());
        while (! token.empty() && (token.back() == '|' || token.back() == ':')) token.pop_back();
        if (token.empty()) continue;
        found = true;
        if (token.front() < 'A' || token.front() > 'G') return false;
        for (std::size_t i = 1; i < token.size(); ++i)
        {
            const auto c = token[i];
            if (! std::isalnum(static_cast<unsigned char>(c)) && c != '#' && c != 'b' && c != '/'
                && c != '+' && c != '-' && c != '(' && c != ')') return false;
        }
    }
    return found;
}

bool isDividerLine(const std::string& text)
{
    auto visible = std::size_t { 0 };
    for (const auto character : text)
    {
        if (std::isspace(static_cast<unsigned char>(character))) continue;
        if (character != '-' && character != '_' && character != '=') return false;
        ++visible;
    }
    return visible >= 3;
}

struct DisplayBlock
{
    int sourceIndex = 0;
    juce::String chords;
    juce::String lyrics;
    juce::String section;
};

std::string transposedRoot(char root, char accidental, int semitones)
{
    static constexpr int naturalPitches[] { 9, 11, 0, 2, 4, 5, 7 };
    static constexpr const char* sharpNames[] { "C", "C#", "D", "D#", "E", "F",
                                                "F#", "G", "G#", "A", "A#", "B" };
    auto pitch = naturalPitches[root - 'A'];
    if (accidental == '#') ++pitch;
    else if (accidental == 'b') --pitch;
    pitch = (pitch + semitones) % 12;
    if (pitch < 0) pitch += 12;
    return sharpNames[pitch];
}

std::string transposeChordToken(const std::string& token, int semitones)
{
    if (token.empty() || token.front() < 'A' || token.front() > 'G' || semitones == 0) return token;
    std::string result;
    std::size_t position = 1;
    const auto accidental = position < token.size() && (token[position] == '#' || token[position] == 'b')
                                ? token[position++] : '\0';
    result += transposedRoot(token.front(), accidental, semitones);
    while (position < token.size())
    {
        if (token[position] == '/' && position + 1 < token.size()
            && token[position + 1] >= 'A' && token[position + 1] <= 'G')
        {
            result += '/';
            const auto bassRoot = token[++position];
            ++position;
            const auto bassAccidental = position < token.size() && (token[position] == '#' || token[position] == 'b')
                                            ? token[position++] : '\0';
            result += transposedRoot(bassRoot, bassAccidental, semitones);
        }
        else
        {
            result += token[position++];
        }
    }
    return result;
}

juce::String transposeChordLine(const juce::String& line, int semitones)
{
    const auto source = line.toStdString();
    std::string output;
    for (std::size_t position = 0; position < source.size();)
    {
        if (std::isspace(static_cast<unsigned char>(source[position])))
        {
            output += source[position++];
            continue;
        }
        auto end = position;
        while (end < source.size() && ! std::isspace(static_cast<unsigned char>(source[end]))) ++end;
        output += transposeChordToken(source.substr(position, end - position), semitones);
        position = end;
    }
    return juce::String(output);
}
}

DisplayView::DisplayView(VocLyricsAudioProcessor& p) : processor(p)
{
    addAndMakeVisible(proportional);
    addAndMakeVisible(clock);
    addAndMakeVisible(autoScroll);
    addAndMakeVisible(autoScrollSpeed);
    addAndMakeVisible(targetKeyChoice);
    addAndMakeVisible(upButton);
    addAndMakeVisible(downButton);
    upButton.onClick = [this] { processor.moveSharedScrollLine(-2); };
    downButton.onClick = [this] { processor.moveSharedScrollLine(2); };
    autoScrollSpeed.setRange(1.0, 60.0, 1.0);
    autoScrollSpeed.setValue(processor.getAutoScrollLinesPerMinute(), juce::dontSendNotification);
    autoScrollSpeed.setTextValueSuffix(" lines/min");
    autoScrollSpeed.setSliderStyle(juce::Slider::LinearHorizontal);
    autoScrollSpeed.setTextBoxStyle(juce::Slider::TextBoxRight, false, 105, 24);
    autoScrollSpeed.onValueChange = [this]
    {
        processor.setAutoScrollLinesPerMinute(juce::roundToInt(autoScrollSpeed.getValue()));
    };
    autoScroll.onClick = [this]
    {
        processor.setAutoScrollEnabled(autoScroll.getToggleState());
        autoScroll.setToggleState(processor.isAutoScrollEnabled(), juce::dontSendNotification);
    };
    targetKeyChoice.setTextWhenNothingSelected("Target key");
    for (int pitch = 0; pitch < 12; ++pitch)
        targetKeyChoice.addItem(juce::String(voclyrics::keyNameForPitchClass(pitch).c_str()), pitch + 1);
    targetKeyChoice.onChange = [this]
    {
        const auto model = processor.getSetlistSnapshot();
        const auto* song = model.activeSong();
        const auto sourcePitch = song != nullptr ? voclyrics::songKeyPitchClass(*song) : -1;
        const auto targetPitch = targetKeyChoice.getSelectedId() - 1;
        if (sourcePitch >= 0 && targetPitch >= 0)
            processor.setTransposeSemitones(voclyrics::transposeSemitonesForKeys(sourcePitch, targetPitch));
    };
    for (auto* button : { &upButton, &downButton })
    {
        button->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1b2937));
        button->setColour(juce::TextButton::textColourOffId, juce::Colour(0xff57e0b5));
    }
    startTimerHz(20);
}
void DisplayView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff080b12));
    juce::ColourGradient glow(juce::Colour(0x4439d9b0), getWidth() * 0.5f, 0.0f,
                              juce::Colour(0x00101828), getWidth() * 0.5f, getHeight() * 0.65f, false);
    g.setGradientFill(glow);
    g.fillRect(getLocalBounds());

    g.setColour(juce::Colour(0xff92a0b5));
    g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    g.drawText(roleText + "  |  " + keyText + "  |  " + transposeText,
               40, 42, getWidth() - 80, 22, juce::Justification::centred);
    g.setColour(juce::Colour(0xfff5f7fa));
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText(titleText, 40, 66, getWidth() - 80, 30, juce::Justification::centred);

    auto sheet = getLocalBounds().reduced(38, 100);
    sheet.removeFromRight(76);
    g.setColour(juce::Colour(0xfffffdf8));
    g.fillRoundedRectangle(sheet.toFloat(), 22.0f);
    g.setColour(juce::Colour(0x22000000));
    g.drawRoundedRectangle(sheet.toFloat(), 22.0f, 1.0f);

    auto sheetHeader = sheet.reduced(30, 18).removeFromTop(28);
    auto sectionBadge = sheetHeader.removeFromLeft(juce::jmin(220, sheetHeader.getWidth() / 2));
    g.setColour(juce::Colour(0xffe9eef3));
    g.fillRoundedRectangle(sectionBadge.toFloat(), 6.0f);
    g.setColour(juce::Colour(0xff334155));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawFittedText(sectionText.isNotEmpty() ? sectionText : "SONG", sectionBadge.reduced(9, 2),
                     juce::Justification::centredLeft, 1);

    auto currentArea = sheet.reduced(30, 54);
    currentArea.removeFromTop(8);
    const auto rowHeight = juce::jmax(1, currentArea.getHeight() / static_cast<int>(visibleLyrics.size()));
    const auto lyricSize = proportional.getToggleState()
        ? juce::jlimit(17.0f, 38.0f, static_cast<float>(rowHeight) * 0.42f)
        : juce::jlimit(17.0f, 32.0f, static_cast<float>(rowHeight) * 0.36f);
    const auto drawRows = [&](const std::array<juce::String, 5>& chords,
                              const std::array<juce::String, 5>& lyricLines, float offset)
    {
        auto area = currentArea.translated(0, juce::roundToInt(offset));
        const juce::Graphics::ScopedSaveState save(g);
        g.reduceClipRegion(currentArea);
        for (std::size_t i = 0; i < lyricLines.size(); ++i)
        {
            auto row = area.removeFromTop(rowHeight);
            auto chordArea = row.removeFromTop(juce::jlimit(18, 31, row.getHeight() / 3));
            if (i == 0)
            {
                g.setColour(juce::Colour(0x1658e0b6));
                g.fillRoundedRectangle(juce::Rectangle<float>(static_cast<float>(sheet.getX() + 12),
                                                              static_cast<float>(chordArea.getY() - 4),
                                                              static_cast<float>(sheet.getWidth() - 24),
                                                              static_cast<float>(rowHeight - 2)), 9.0f);
            }
            g.setColour(juce::Colour(0xffd94712));
            g.setFont(juce::FontOptions(juce::jlimit(15.0f, 25.0f, lyricSize * 0.72f), juce::Font::bold));
            g.drawFittedText(chords[i], chordArea, juce::Justification::centredLeft, 1);
            g.setColour(juce::Colour(0xff111827));
            g.setFont(juce::FontOptions(lyricSize, i == 0 ? juce::Font::bold : juce::Font::plain));
            g.drawFittedText(lyricLines[i], row, juce::Justification::centredLeft, 1);
        }
    };
    if (animationStartMilliseconds > 0.0)
    {
        const auto elapsed = juce::Time::getMillisecondCounterHiRes() - animationStartMilliseconds;
        const auto linear = juce::jlimit(0.0f, 1.0f, static_cast<float>(elapsed / 420.0));
        const auto eased = 1.0f - std::pow(1.0f - linear, 3.0f);
        const auto distance = static_cast<float>(rowHeight * animationRows * animationDirection);
        drawRows(previousChords, previousLyrics, -distance * eased);
        drawRows(visibleChords, visibleLyrics, distance * (1.0f - eased));
    }
    else drawRows(visibleChords, visibleLyrics, 0.0f);
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
    autoScroll.setBounds(top.removeFromLeft(120));
    autoScrollSpeed.setBounds(top.removeFromLeft(240));
    targetKeyChoice.setBounds(top.removeFromLeft(120));
    auto controls = getLocalBounds().removeFromRight(82).withSizeKeepingCentre(62, 154);
    upButton.setBounds(controls.removeFromTop(70));
    controls.removeFromTop(14);
    downButton.setBounds(controls.removeFromTop(70));
}
void DisplayView::timerCallback()
{
    const auto model = processor.getSetlistSnapshot();
    const auto* song = model.activeSong();
    const auto songId = song != nullptr ? juce::String(song->id.c_str()) : juce::String();
    if (songId != displayedSongId)
    {
        displayedSongId = songId;
        displaySourceIndex = -1;
        animationStartMilliseconds = 0.0;
    }
    titleText = song != nullptr ? song->title : "VocLyrics";
    transposeText = juce::String("TRANSPOSED BY: ") + juce::String(processor.getTransposeSemitones());
    const auto sourcePitch = song != nullptr ? voclyrics::songKeyPitchClass(*song) : -1;
    const auto targetPitch = sourcePitch >= 0 ? (sourcePitch + processor.getTransposeSemitones() + 120) % 12 : -1;
    targetKeyChoice.setEnabled(sourcePitch >= 0);
    targetKeyChoice.setSelectedId(targetPitch >= 0 ? targetPitch + 1 : 0, juce::dontSendNotification);
    keyText = sourcePitch >= 0
        ? juce::String("Key- ") + voclyrics::keyNameForPitchClass(sourcePitch).c_str() + " → "
            + voclyrics::keyNameForPitchClass(targetPitch).c_str()
        : juce::String("Add Key- A#");
    roleText = processor.getHostRole().toUpperCase();
    sectionText = "SONG";
    std::array<juce::String, 5> nextChords, nextLyrics;
    auto nextSourceIndex = -1;
    if (song != nullptr && ! song->lyrics.empty())
    {
        auto position = processor.getTransportSnapshot().positionSeconds;
        if (song->startSeconds >= 0.0) position -= song->startSeconds;
        auto active = processor.getSharedScrollLine();
        if (active < 0) active = voclyrics::activeLineAt(*song, position);
        if (active < 0) active = 0;
        active = juce::jlimit(0, static_cast<int>(song->lyrics.size()) - 1, active);
        std::vector<DisplayBlock> blocks;
        juce::String currentSection { "SONG" };
        for (int i = 0; i < static_cast<int>(song->lyrics.size()); ++i)
        {
            const auto& line = song->lyrics[static_cast<std::size_t>(i)];
            if (line.isSection)
            {
                currentSection = juce::String(line.text).toUpperCase();
                continue;
            }
            if (isDividerLine(line.text) || voclyrics::isSongKeyLine(line.text)) continue;
            DisplayBlock block;
            block.sourceIndex = i;
            block.section = currentSection;
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
        if (! blocks.empty())
        {
            sectionText = blocks[start].section;
            nextSourceIndex = static_cast<int>(start);
        }
        for (std::size_t i = 0; i < visibleLyrics.size() && start + i < blocks.size(); ++i)
        {
            nextChords[i] = transposeChordLine(blocks[start + i].chords, processor.getTransposeSemitones());
            nextLyrics[i] = blocks[start + i].lyrics;
        }
    }
    if (nextSourceIndex != displaySourceIndex)
    {
        if (displaySourceIndex >= 0 && nextSourceIndex >= 0)
        {
            previousChords = visibleChords;
            previousLyrics = visibleLyrics;
            animationDirection = nextSourceIndex > displaySourceIndex ? 1 : -1;
            animationRows = juce::jlimit(1, 5, std::abs(nextSourceIndex - displaySourceIndex));
            animationStartMilliseconds = juce::Time::getMillisecondCounterHiRes();
        }
        displaySourceIndex = nextSourceIndex;
    }
    visibleChords = std::move(nextChords);
    visibleLyrics = std::move(nextLyrics);
    if (animationStartMilliseconds > 0.0
        && juce::Time::getMillisecondCounterHiRes() - animationStartMilliseconds >= 420.0)
        animationStartMilliseconds = 0.0;
    clockText = juce::Time::getCurrentTime().formatted("%H:%M");
    const auto hostControls = processor.hostControlsSharedScroll();
    autoScroll.setToggleState(processor.isAutoScrollEnabled(), juce::dontSendNotification);
    autoScroll.setEnabled(hostControls);
    autoScrollSpeed.setValue(processor.getAutoScrollLinesPerMinute(), juce::dontSendNotification);
    autoScrollSpeed.setEnabled(hostControls);
    upButton.setEnabled(hostControls);
    downButton.setEnabled(hostControls);
    repaint();
}
