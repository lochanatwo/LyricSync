#pragma once

#include "PluginProcessor.h"
#include <JuceHeader.h>
#include <vector>

class RemotePanel final : public juce::Component, private juce::Timer
{
public:
    explicit RemotePanel(VocLyricsAudioProcessor& processor);
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    class DeviceList final : public juce::Component
    {
    public:
        explicit DeviceList(RemotePanel& owner) : parent(owner) {}
        void paint(juce::Graphics&) override;
        void mouseDown(const juce::MouseEvent&) override;
    private:
        RemotePanel& parent;
    };
    void timerCallback() override;
    void refreshDevices();
    VocLyricsAudioProcessor& processor;
    juce::ToggleButton enabled { "Server on" };
    juce::Label address, portLabel, controllerLabel, roleLabel;
    juce::ComboBox interfaceChoice;
    juce::ComboBox controllerChoice;
    juce::ComboBox roleChoice;
    juce::TextEditor port;
    juce::TextButton applyPort { "Apply" };
    DeviceList deviceList;
    juce::Viewport viewport;
    std::vector<voclyrics::RemoteDevice> currentDevices;
};
