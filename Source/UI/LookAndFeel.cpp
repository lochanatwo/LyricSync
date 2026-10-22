#include "LookAndFeel.h"

namespace voclyrics
{
LookAndFeel::LookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff080b12));
    setColour(juce::Label::textColourId, juce::Colour(0xffe8edf5));
    setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff111925));
    setColour(juce::TextEditor::textColourId, juce::Colour(0xfff1f5f9));
    setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff344258));
    setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(0xff58e0b6));
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff202c3c));
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff16745f));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff151f2d));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff3a4960));
    setColour(juce::ComboBox::arrowColourId, juce::Colour(0xff58e0b6));
}
}
