#include "DisplayView.h"
#include "Lyrics/ChordTransposer.h"
#include "Lyrics/LrcParser.h"
#include <cmath>
#include <cctype>
#include <utility>
#include <vector>

namespace
{
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

}

DisplayView::DisplayView(VocLyricsAudioProcessor& p) : processor(p)
{
    setWantsKeyboardFocus(true);
    addAndMakeVisible(proportional);
    addAndMakeVisible(clock);
    addAndMakeVisible(autoScroll);
    addAndMakeVisible(tapTempoButton);
    addAndMakeVisible(stopTempoButton);
    addAndMakeVisible(useManualBpmButton);
    addAndMakeVisible(tapTempoStatus);
    addAndMakeVisible(manualTempoBpm);
    addAndMakeVisible(autoScrollSpeed);
    addAndMakeVisible(sourceKeyChoice);
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
    tapTempoButton.onClick = [this] { processor.registerTapTempo(); };
    stopTempoButton.onClick = [this] { processor.stopTapTempo(); };
    manualTempoBpm.setRange(8.0, 300.0, 1.0);
    manualTempoBpm.setValue(processor.getManualTempoBpm(), juce::dontSendNotification);
    manualTempoBpm.setTextValueSuffix(" BPM");
    manualTempoBpm.setSliderStyle(juce::Slider::LinearHorizontal);
    manualTempoBpm.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 76, 24);
    useManualBpmButton.onClick = [this]
    {
        processor.setManualTempoBpm(juce::roundToInt(manualTempoBpm.getValue()));
    };
    tapTempoStatus.setJustificationType(juce::Justification::centredLeft);
    sourceKeyChoice.addItem("Source: file", 1);
    for (int pitch = 0; pitch < 12; ++pitch)
        sourceKeyChoice.addItem(juce::String("Source: ") + voclyrics::keyNameForPitchClass(pitch).c_str(), pitch + 2);
    sourceKeyChoice.onChange = [this]
    {
        processor.setSourceKeyForActiveSong(sourceKeyChoice.getSelectedId() - 2);
    };
    targetKeyChoice.setTextWhenNothingSelected("Target key");
    for (int pitch = 0; pitch < 12; ++pitch)
        targetKeyChoice.addItem(juce::String("Target: ") + voclyrics::keyNameForPitchClass(pitch).c_str(), pitch + 1);
    targetKeyChoice.onChange = [this]
    {
        const auto model = processor.getSetlistSnapshot();
        const auto* song = model.activeSong();
        const auto sourcePitch = song != nullptr ? voclyrics::songKeyPitchClass(*song) : -1;
        const auto targetPitch = targetKeyChoice.getSelectedId() - 1;
        if (sourcePitch >= 0 && targetPitch >= 0)
            processor.setTargetKeyForActiveSong(targetPitch);
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
               40, 76, getWidth() - 80, 22, juce::Justification::centred);
    g.setColour(juce::Colour(0xfff5f7fa));
    g.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    g.drawText(titleText, 40, 100, getWidth() - 80, 30, juce::Justification::centred);

    auto sheet = getLocalBounds().reduced(20);
    sheet.removeFromTop(112);
    sheet.removeFromBottom(18);
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
    currentArea.removeFromRight(48);
    const auto rowHeight = juce::jmax(1, currentArea.getHeight() / static_cast<int>(visibleLyrics.size()));
    const auto lyricSize = proportional.getToggleState()
        ? juce::jlimit(15.0f, 31.0f, static_cast<float>(rowHeight) * 0.35f)
        : juce::jlimit(15.0f, 27.0f, static_cast<float>(rowHeight) * 0.31f);
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
            g.setFont(juce::FontOptions(juce::jlimit(13.0f, 21.0f, lyricSize * 0.70f), juce::Font::bold));
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
    proportional.setBounds(top.removeFromLeft(125));
    clock.setBounds(top.removeFromLeft(75));
    autoScroll.setBounds(top.removeFromLeft(105));
    autoScrollSpeed.setBounds(top.removeFromLeft(200));
    tapTempoButton.setBounds(top.removeFromLeft(95));
    stopTempoButton.setBounds(top.removeFromLeft(50));
    tapTempoStatus.setBounds(top.removeFromLeft(100));
    auto tempoAndKey = getLocalBounds();
    tempoAndKey.removeFromTop(40);
    tempoAndKey = tempoAndKey.removeFromTop(34).reduced(8, 4);
    manualTempoBpm.setBounds(tempoAndKey.removeFromLeft(175));
    useManualBpmButton.setBounds(tempoAndKey.removeFromLeft(92));
    sourceKeyChoice.setBounds(tempoAndKey.removeFromLeft(115));
    targetKeyChoice.setBounds(tempoAndKey.removeFromLeft(120));
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
        manualTempoBpm.setValue(processor.getManualTempoBpm(), juce::dontSendNotification);
    }
    titleText = song != nullptr ? song->title : "VocLyrics";
    transposeText = juce::String("TRANSPOSED BY: ") + juce::String(processor.getTransposeSemitones());
    const auto sourcePitch = song != nullptr ? voclyrics::songKeyPitchClass(*song) : -1;
    sourceKeyChoice.setSelectedId(song != nullptr ? song->sourceKeyOverride + 2 : 1, juce::dontSendNotification);
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
            if (voclyrics::isChordOnlyLine(line.text) && i + 1 < static_cast<int>(song->lyrics.size())
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
            nextChords[i] = juce::String(voclyrics::transposeChordLine(
                blocks[start + i].chords.toStdString(), processor.getTransposeSemitones()));
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
    const auto tapActive = processor.isTapTempoEnabled();
    const auto bpm = processor.getTapTempoBpm();
    tapTempoStatus.setText(bpm > 0 ? juce::String(bpm) + " BPM / bar"
                                  : (tapActive ? juce::String("Tap ") + juce::String(juce::jmin(4, processor.getTapTempoTapCount())) + "/4"
                                               : juce::String("Shift+Space")),
                           juce::dontSendNotification);
    tapTempoButton.setEnabled(hostControls);
    useManualBpmButton.setEnabled(hostControls);
    stopTempoButton.setEnabled(tapActive);
    upButton.setEnabled(hostControls);
    downButton.setEnabled(hostControls);
    repaint();
}
