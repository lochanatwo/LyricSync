#include "LookAndFeel.h"

namespace voclyrics
{
LookAndFeel::LookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(0xff11151c));
    setColour(juce::Label::textColourId, juce::Colour(0xffe8edf5));
    setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff181e27));
    setColour(juce::TextEditor::textColourId, juce::Colour(0xfff1f5f9));
    setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff303b4a));
    setColour(juce::TextButton::buttonColourId, juce::Colour(0xff263243));
    setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff136f63));
    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff181e27));
}
}
