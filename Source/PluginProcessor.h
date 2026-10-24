#pragma once

#include <JuceHeader.h>
#include "Lyrics/SetlistModel.h"
#include "Network/ServerManager.h"
#include <atomic>
#include <cstdint>
#include <string>

static_assert(std::atomic<std::int64_t>::is_always_lock_free,
              "VocLyrics requires lock-free 64-bit atomics for audio transport state");
static_assert(std::atomic<bool>::is_always_lock_free,
              "VocLyrics requires lock-free boolean atomics for audio transport state");

class VocLyricsAudioProcessor final : public juce::AudioProcessor, private juce::Timer
{
public:
    struct TransportSnapshot
    {
        double positionSeconds = 0.0;
        bool hasPosition = false;
        bool isPlaying = false;
        bool isLooping = false;
    };

    VocLyricsAudioProcessor();
    ~VocLyricsAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    TransportSnapshot getTransportSnapshot() const noexcept;
    voclyrics::SetlistModel getSetlistSnapshot() const;
    void selectSong(int index);
    int addSong();
    bool duplicateSong(int index);
    bool deleteSong(int index);
    bool moveSong(int from, int to);
    void renameSong(int index, const juce::String& title);
    void setSongMarker(int index, double seconds);
    void setSongAutoAdvance(int index, bool enabled);
    void setLyricsText(const juce::String& text);
    juce::String getLyricsText() const;
    bool tagNextLine();
    bool undoTag();
    void jumpToSong(int index);
    voclyrics::ServerManager& getServer() noexcept { return server; }
    int getActiveIndex() const;
    void setSharedScrollLine(int line);
    int getTransposeSemitones() const noexcept { return transposeSemitones.load(std::memory_order_relaxed); }
    void setTransposeSemitones(int semitones) noexcept
    {
        transposeSemitones.store(juce::jlimit(-12, 12, semitones), std::memory_order_relaxed);
    }
    juce::String getHostRole() const;
    void setHostRole(const juce::String& role);
    int getSharedScrollLine() const { return server.scrollLine(); }
    bool hostControlsSharedScroll() const { return server.scrollController() == 0; }
    void moveSharedScrollLine(int direction);
    void setSharedScrollController(int id);
    bool isAutoScrollEnabled() const noexcept { return autoScrollEnabled.load(std::memory_order_relaxed); }
    void setAutoScrollEnabled(bool enabled);
    int getAutoScrollLinesPerMinute() const noexcept { return autoScrollLinesPerMinute.load(std::memory_order_relaxed); }
    void setAutoScrollLinesPerMinute(int linesPerMinute);

private:
    void publishTransport() noexcept;
    void timerCallback() override;
    void updateServerState();

    // Integer microseconds avoid relying on potentially non-lock-free floating
    // point atomics. The audio callback only publishes these scalar values.
    std::atomic<std::int64_t> positionMicros { 0 };
    std::atomic<bool> hasPosition { false };
    std::atomic<bool> isPlaying { false };
    std::atomic<bool> isLooping { false };
    std::atomic<int> transposeSemitones { 0 };
    std::atomic<bool> autoScrollEnabled { false };
    std::atomic<int> autoScrollLinesPerMinute { 12 };

    mutable juce::CriticalSection modelLock;
    voclyrics::SetlistModel setlist;
    voclyrics::ServerManager server;
    int lastTaggedIndex = -1;
    double previousTagTime = -1.0;
    int preferredPort = 8080;
    std::string lastAutoAdvancedId;
    juce::String hostRole { "Main Singer" };
    double autoScrollLastTickMilliseconds = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VocLyricsAudioProcessor)
};
