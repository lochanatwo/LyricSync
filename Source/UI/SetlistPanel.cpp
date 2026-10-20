#include "SetlistPanel.h"

SetlistPanel::SetlistPanel(VocLyricsAudioProcessor& p) : processor(p)
{
    for (juce::TextButton* button : {&add, &duplicate, &remove, &marker, &up, &down, &previous, &next})
        addAndMakeVisible(*button);
    addAndMakeVisible(autoAdvance);
    add.onClick = [this] { processor.addSong(); refresh(); if (onSelectionChanged) onSelectionChanged(); };
    duplicate.onClick = [this] { processor.duplicateSong(processor.getActiveIndex()); refresh(); if (onSelectionChanged) onSelectionChanged(); };
    remove.onClick = [this] { processor.deleteSong(processor.getActiveIndex()); refresh(); if (onSelectionChanged) onSelectionChanged(); };
    up.onClick = [this] { const auto i = processor.getActiveIndex(); processor.moveSong(i, juce::jmax(0, i - 1)); refresh(); };
    down.onClick = [this] { const auto i = processor.getActiveIndex(); processor.moveSong(i, i + 1); refresh(); };
    previous.onClick = [this] { processor.selectSong(juce::jmax(0, processor.getActiveIndex() - 1)); refresh(); if (onSelectionChanged) onSelectionChanged(); };
    next.onClick = [this] { processor.selectSong(processor.getActiveIndex() + 1); refresh(); if (onSelectionChanged) onSelectionChanged(); };
    marker.onClick = [this] { processor.setSongMarker(processor.getActiveIndex(), processor.getTransportSnapshot().positionSeconds); refresh(); };
    autoAdvance.onClick = [this] { processor.setSongAutoAdvance(processor.getActiveIndex(), autoAdvance.getToggleState()); };
}

void SetlistPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff151a22));
    g.setColour(juce::Colour(0xff8793a3));
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("SETLIST", 14, 10, getWidth() - 28, 24, juce::Justification::centredLeft);
    const auto model = processor.getSetlistSnapshot();
    int y = 44;
    const auto active = model.activeIndex();
    for (std::size_t i = 0; i < model.songs().size(); ++i, y += 54)
    {
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
        if (model.songs()[i].startSeconds >= 0.0)
        {
            g.setColour(juce::Colour(0xff8490a0));
            g.setFont(juce::FontOptions(10.0f));
            const auto markerText = "Marker " + juce::String(model.songs()[i].startSeconds, 1) + "s";
            g.drawText(markerText, bounds.getRight() - 74, bounds.getY() + 27, 65, 14, juce::Justification::centredRight);
        }
        if (static_cast<int>(i) == active + 1)
        {
            g.setColour(juce::Colour(0xff8490a0));
            g.setFont(juce::FontOptions(11.0f));
            g.drawText("NEXT", bounds.getX() + 10, bounds.getBottom() - 17, bounds.getWidth() - 18, 14, juce::Justification::centredLeft);
        }
    }
}

void SetlistPanel::resized()
{
    auto area = getLocalBounds().reduced(8);
    area.removeFromTop(40);
    auto bottom = area.removeFromBottom(138);
    auto row = bottom.removeFromTop(34);
    add.setBounds(row.removeFromLeft(row.getWidth() / 2).reduced(2));
    duplicate.setBounds(row.reduced(2));
    row = bottom.removeFromTop(34);
    remove.setBounds(row.removeFromLeft(row.getWidth() / 2).reduced(2));
    auto moveControls = row.reduced(2);
    up.setBounds(moveControls.removeFromLeft(moveControls.getWidth() / 2).reduced(2));
    down.setBounds(moveControls.reduced(2));
    row = bottom.removeFromTop(34);
    marker.setBounds(row.removeFromLeft(row.getWidth() * 0.58f).reduced(2));
    autoAdvance.setBounds(row.reduced(2));
    row = bottom.removeFromTop(32);
    previous.setBounds(row.removeFromLeft(row.getWidth() / 2).reduced(2));
    next.setBounds(row.reduced(2));
}

void SetlistPanel::refresh()
{
    const auto model = processor.getSetlistSnapshot();
    const auto* active = model.activeSong();
    autoAdvance.setToggleState(active != nullptr && active->autoAdvance, juce::dontSendNotification);
    repaint();
}
void SetlistPanel::mouseDown(const juce::MouseEvent& event)
{
    const auto row = (event.y - 44) / 54;
    const auto model = processor.getSetlistSnapshot();
    pressedRow = row >= 0 && row < static_cast<int>(model.songs().size()) ? row : -1;
    dragged = false;
}
void SetlistPanel::mouseDrag(const juce::MouseEvent& event)
{
    if (pressedRow < 0 || event.getDistanceFromDragStart() < 4) return;
    const auto row = juce::jlimit(0, static_cast<int>(processor.getSetlistSnapshot().songs().size()) - 1,
                                  (event.y - 44) / 54);
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
    const auto row = (event.y - 44) / 54;
    const auto model = processor.getSetlistSnapshot();
    if (row < 0 || row >= static_cast<int>(model.songs().size())) return;
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

