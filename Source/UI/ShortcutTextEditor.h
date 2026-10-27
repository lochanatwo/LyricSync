#pragma once

#include <JuceHeader.h>
#include <functional>

class ShortcutTextEditor final : public juce::TextEditor
{
public:
    std::function<bool(const juce::KeyPress&)> onShortcut;

    bool keyPressed(const juce::KeyPress& key) override
    {
        if (onShortcut && onShortcut(key)) return true;
        return juce::TextEditor::keyPressed(key);
    }
};
