#include "EditorPanel.h"
#include <vector>
#include <limits>

namespace
{
std::vector<int> nonEmptyLineStarts(const juce::String& text)
{
    std::vector<int> starts;
    int offset = 0;
    while (offset <= text.length())
    {
        const auto foundEnd = text.indexOf(offset, "\n");
        const auto end = foundEnd < 0 ? text.length() : foundEnd;
        if (text.substring(offset, end).trim().isNotEmpty()) starts.push_back(offset);
        if (end == text.length()) break;
        offset = end + 1;
    }
    return starts;
}

int lineForCaret(const std::vector<int>& starts, int caret)
{
    for (std::size_t i = 0; i < starts.size(); ++i)
    {
        const auto next = i + 1 < starts.size() ? starts[i + 1] : std::numeric_limits<int>::max();
        if (caret >= starts[i] && caret < next) return static_cast<int>(i);
    }
    return starts.empty() ? -1 : 0;
}
}

EditorPanel::EditorPanel(VocLyricsAudioProcessor& p) : processor(p)
{
    lyrics.setMultiLine(true, true);
    lyrics.setReturnKeyStartsNewLine(true);
    lyrics.setFont(juce::FontOptions(19.0f));
    lyrics.setScrollbarsShown(true);
    lyrics.onTextChange = [this] { processor.setLyricsText(lyrics.getText()); };
    for (auto* button : {&tag, &undo, &import, &exportFile, &previousSection, &nextSection, &previousLine, &nextLine})
        addAndMakeVisible(*button);
    addAndMakeVisible(lyrics);
    addAndMakeVisible(shareScroll);
    tag.onClick = [this] { processor.tagNextLine(); loadFromProcessor(); };
    undo.onClick = [this] { processor.undoTag(); loadFromProcessor(); };
    previousSection.onClick = [this] { moveSection(-1); };
    nextSection.onClick = [this] { moveSection(1); };
    previousLine.onClick = [this] { moveCaretLine(-1); };
    nextLine.onClick = [this] { moveCaretLine(1); };
    shareScroll.onClick = [this]
    {
        if (shareScroll.getToggleState()) timerCallback();
        else processor.setSharedScrollLine(-1);
    };
    import.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser>("Import lyrics", juce::File(), "*.lrc");
        const juce::Component::SafePointer<EditorPanel> safe(this);
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe](const juce::FileChooser& fileChooser)
            {
                if (safe == nullptr) return;
                const auto file = fileChooser.getResult();
                if (file.existsAsFile()) { safe->lyrics.setText(file.loadFileAsString(), false); safe->processor.setLyricsText(safe->lyrics.getText()); }
            });
    };
    exportFile.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser>("Export lyrics", juce::File("Lyrics.lrc"), "*.lrc");
        const juce::Component::SafePointer<EditorPanel> safe(this);
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
            [safe](const juce::FileChooser& fileChooser)
            {
                if (safe == nullptr) return;
                const auto file = fileChooser.getResult();
                if (file != juce::File()) file.replaceWithText(safe->processor.getLyricsText());
            });
    };
    loadFromProcessor();
    startTimerHz(12);
}
void EditorPanel::resized()
{
    auto area = getLocalBounds().reduced(12);
    auto controls = area.removeFromTop(42);
    tag.setBounds(controls.removeFromLeft(145).reduced(3));
    undo.setBounds(controls.removeFromLeft(110).reduced(3));
    import.setBounds(controls.removeFromLeft(105).reduced(3));
    exportFile.setBounds(controls.removeFromLeft(105).reduced(3));
    previousSection.setBounds(controls.removeFromLeft(105).reduced(3));
    nextSection.setBounds(controls.removeFromLeft(105).reduced(3));
    auto sharedControls = area.removeFromTop(38);
    shareScroll.setBounds(sharedControls.removeFromLeft(130).reduced(3));
    previousLine.setBounds(sharedControls.removeFromLeft(125).reduced(3));
    nextLine.setBounds(sharedControls.removeFromLeft(125).reduced(3));
    area.removeFromTop(4);
    lyrics.setBounds(area);
}

void EditorPanel::moveCaretLine(int direction)
{
    const auto text = lyrics.getText();
    const auto starts = nonEmptyLineStarts(text);
    if (starts.empty()) return;
    const auto current = lineForCaret(starts, lyrics.getCaretPosition());
    const auto target = juce::jlimit(0, static_cast<int>(starts.size()) - 1, current + direction);
    lyrics.setCaretPosition(starts[static_cast<std::size_t>(target)]);
    shareScroll.setToggleState(true, juce::sendNotification);
    processor.setSharedScrollLine(target);
}

void EditorPanel::timerCallback()
{
    if (! shareScroll.getToggleState()) return;
    const auto text = lyrics.getText();
    processor.setSharedScrollLine(lineForCaret(nonEmptyLineStarts(text), lyrics.getCaretPosition()));
}

void EditorPanel::moveSection(int direction)
{
    const auto text = lyrics.getText();
    const auto caret = lyrics.getCaretPosition();
    std::vector<int> starts;
    int offset = 0;
    while (offset < text.length())
    {
        const auto end = text.indexOf(offset, "\n");
        const auto lineEnd = end < 0 ? text.length() : end;
        const auto line = text.substring(offset, lineEnd).trim();
        if (line.startsWithChar('[') && line.endsWithChar(']') && ! line.containsChar(':'))
            starts.push_back(offset);
        offset = lineEnd + 1;
    }
    if (starts.empty()) return;
    if (direction > 0)
    {
        for (const auto start : starts) if (start > caret) { lyrics.setCaretPosition(start); return; }
        lyrics.setCaretPosition(starts.front());
    }
    else
    {
        for (auto it = starts.rbegin(); it != starts.rend(); ++it) if (*it < caret) { lyrics.setCaretPosition(*it); return; }
        lyrics.setCaretPosition(starts.back());
    }
}
void EditorPanel::loadFromProcessor()
{
    const auto text = processor.getLyricsText();
    if (lyrics.getText() != text) lyrics.setText(text, false);
}
