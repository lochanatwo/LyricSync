#include "SetlistPanel.h"

SetlistPanel::SetlistPanel(VocLyricsAudioProcessor& p) : processor(p)
{
    addAndMakeVisible(search);
    search.setTextToShowWhenEmpty("Search songs  Ctrl+F", juce::Colour(0xff738095));
    search.setJustification(juce::Justification::centredLeft);
    search.onTextChange = [this] { pressedRow = -1; refresh(); };
    search.onReturnKey = [this]
    {
        const auto matches = matchingSongIndices();
        if (matches.empty()) return;
        processor.selectSong(matches.front());
        refresh();
        if (onSelectionChanged) onSelectionChanged();
    };
    search.onEscapeKey = [this] { search.clear(); };
    for (juce::TextButton* button : {&add, &duplicate, &remove, &up, &down, &previous, &next})
        addAndMakeVisible(*button);
    add.onClick = [this] { processor.addSong(); refresh(); if (onSelectionChanged) onSelectionChanged(); };
    duplicate.onClick = [this] { processor.duplicateSong(processor.getActiveIndex()); refresh(); if (onSelectionChanged) onSelectionChanged(); };
    remove.onClick = [this] { processor.deleteSong(processor.getActiveIndex()); refresh(); if (onSelectionChanged) onSelectionChanged(); };
    up.onClick = [this] { const auto i = processor.getActiveIndex(); processor.moveSong(i, juce::jmax(0, i - 1)); refresh(); };
    down.onClick = [this] { const auto i = processor.getActiveIndex(); processor.moveSong(i, i + 1); refresh(); };
    previous.onClick = [this] { processor.selectSong(juce::jmax(0, processor.getActiveIndex() - 1)); refresh(); if (onSelectionChanged) onSelectionChanged(); };
    next.onClick = [this] { processor.selectSong(processor.getActiveIndex() + 1); refresh(); if (onSelectionChanged) onSelectionChanged(); };
}

void SetlistPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff151a22));
    g.setColour(juce::Colour(0xff8793a3));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("SETLIST", 14, 10, getWidth() - 28, 24, juce::Justification::centredLeft);
    const auto model = processor.getSetlistSnapshot();
    const auto matches = matchingSongIndices();
    int y = 82;
    const auto active = model.activeIndex();
    if (matches.empty())
    {
        g.setColour(juce::Colour(0xff7f8b9d));
        g.setFont(juce::FontOptions(13.0f));
        g.drawFittedText("No matching songs", 12, 90, getWidth() - 24, 32, juce::Justification::centred, 1);
    }
    for (const auto songIndex : matches)
    {
        const auto i = static_cast<std::size_t>(songIndex);
        const auto bounds = juce::Rectangle<int>(8, y, getWidth() - 16, 48);
        if (static_cast<int>(i) == active)
        {
            g.setColour(juce::Colour(0xff17483f));
            g.fillRoundedRectangle(bounds.toFloat(), 8.0f);
            g.setColour(juce::Colour(0xff6ee7b7));
        }
        else
        {
            g.setColour(juce::Colour(0xff252d39));
            g.fillRoundedRectangle(bounds.toFloat(), 8.0f);
            g.setColour(juce::Colour(0xffccd3dd));
        }
        g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        g.drawFittedText(juce::String(model.songs()[i].title.c_str()), bounds.reduced(12, 2),
                         juce::Justification::centredLeft, 1);
        if (static_cast<int>(i) == active + 1)
        {
            g.setColour(juce::Colour(0xff8490a0));
            g.setFont(juce::FontOptions(11.0f));
            g.drawText("NEXT", bounds.getX() + 10, bounds.getBottom() - 17, bounds.getWidth() - 18, 14, juce::Justification::centredLeft);
        }
        y += 54;
    }
}

void SetlistPanel::resized()
{
    auto area = getLocalBounds().reduced(8);
    area.removeFromTop(40);
    search.setBounds(area.removeFromTop(32));
    area.removeFromTop(6);
    auto bottom = area.removeFromBottom(106);
    auto row = bottom.removeFromTop(34);
    add.setBounds(row.removeFromLeft(row.getWidth() / 2).reduced(2));
    duplicate.setBounds(row.reduced(2));
    row = bottom.removeFromTop(34);
    remove.setBounds(row.removeFromLeft(row.getWidth() / 2).reduced(2));
    auto moveControls = row.reduced(2);
    up.setBounds(moveControls.removeFromLeft(moveControls.getWidth() / 2).reduced(2));
    down.setBounds(moveControls.reduced(2));
    row = bottom.removeFromTop(32);
    previous.setBounds(row.removeFromLeft(row.getWidth() / 2).reduced(2));
    next.setBounds(row.reduced(2));
}

void SetlistPanel::refresh()
{
    repaint();
}
void SetlistPanel::focusSearch()
{
    search.grabKeyboardFocus();
    search.selectAll();
}
std::vector<int> SetlistPanel::matchingSongIndices() const
{
    const auto model = processor.getSetlistSnapshot();
    const auto query = search.getText().trim();
    std::vector<int> matches;
    for (int i = 0; i < static_cast<int>(model.songs().size()); ++i)
        if (query.isEmpty() || juce::String(model.songs()[static_cast<std::size_t>(i)].title.c_str()).containsIgnoreCase(query))
            matches.push_back(i);
    return matches;
}
void SetlistPanel::mouseDown(const juce::MouseEvent& event)
{
    if (event.y < 82) { pressedRow = -1; return; }
    const auto row = (event.y - 82) / 54;
    const auto matches = matchingSongIndices();
    pressedRow = row >= 0 && row < static_cast<int>(matches.size()) ? matches[static_cast<std::size_t>(row)] : -1;
    dragged = false;
}
void SetlistPanel::mouseDrag(const juce::MouseEvent& event)
{
    if (pressedRow < 0 || event.getDistanceFromDragStart() < 4 || search.getText().isNotEmpty()) return;
    const auto row = juce::jlimit(0, static_cast<int>(processor.getSetlistSnapshot().songs().size()) - 1,
                                  (event.y - 82) / 54);
    if (row != pressedRow)
    {
        processor.moveSong(pressedRow, row);
        pressedRow = row;
        dragged = true;
        refresh();
    }
}
void SetlistPanel::mouseUp(const juce::MouseEvent&)
{
    if (pressedRow >= 0 && ! dragged)
    {
        processor.selectSong(pressedRow);
        refresh();
        if (onSelectionChanged) onSelectionChanged();
    }
    pressedRow = -1;
}
void SetlistPanel::mouseDoubleClick(const juce::MouseEvent& event)
{
    if (event.y < 82) return;
    const auto visibleRow = (event.y - 82) / 54;
    const auto matches = matchingSongIndices();
    const auto model = processor.getSetlistSnapshot();
    if (visibleRow < 0 || visibleRow >= static_cast<int>(matches.size())) return;
    const auto row = matches[static_cast<std::size_t>(visibleRow)];
    auto* prompt = new juce::AlertWindow("Rename song", "Enter a song title", juce::AlertWindow::NoIcon);
    prompt->addTextEditor("title", model.songs()[static_cast<std::size_t>(row)].title);
    prompt->addButton("Rename", 1);
    prompt->addButton("Cancel", 0);
    const juce::Component::SafePointer<SetlistPanel> safe(this);
    prompt->enterModalState(true, juce::ModalCallbackFunction::create(
        [safe, row, prompt](int result)
        {
            if (safe != nullptr && result == 1)
            {
                safe->processor.renameSong(row, prompt->getTextEditorContents("title"));
                safe->refresh();
            }
        }), true);
}
