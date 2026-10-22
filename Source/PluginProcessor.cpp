#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Lyrics/LiveTagger.h"

#include <cmath>
#include <utility>
#include <vector>

VocLyricsAudioProcessor::VocLyricsAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    server.start(preferredPort);
    startTimerHz(10);
}

void VocLyricsAudioProcessor::prepareToPlay(double, int)
{
    publishTransport();
}

void VocLyricsAudioProcessor::releaseResources()
{
}

bool VocLyricsAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void VocLyricsAudioProcessor::processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&)
{
    publishTransport();
}

void VocLyricsAudioProcessor::processBlock(juce::AudioBuffer<double>&, juce::MidiBuffer&)
{
    publishTransport();
}

void VocLyricsAudioProcessor::publishTransport() noexcept
{
    const auto* playHead = getPlayHead();
    if (playHead == nullptr)
    {
        hasPosition.store(false, std::memory_order_relaxed);
        isPlaying.store(false, std::memory_order_relaxed);
        isLooping.store(false, std::memory_order_relaxed);
        return;
    }

    const auto position = playHead->getPosition();
    if (! position.hasValue())
    {
        hasPosition.store(false, std::memory_order_relaxed);
        isPlaying.store(false, std::memory_order_relaxed);
        isLooping.store(false, std::memory_order_relaxed);
        return;
    }

    const auto seconds = position->getTimeInSeconds();
    if (seconds.hasValue() && std::isfinite(*seconds) && *seconds >= 0.0)
    {
        positionMicros.store(static_cast<std::int64_t>(*seconds * 1000000.0), std::memory_order_relaxed);
        hasPosition.store(true, std::memory_order_relaxed);
    }
    else
    {
        hasPosition.store(false, std::memory_order_relaxed);
    }

    isPlaying.store(position->getIsPlaying(), std::memory_order_relaxed);
    isLooping.store(position->getIsLooping(), std::memory_order_relaxed);
}

VocLyricsAudioProcessor::TransportSnapshot VocLyricsAudioProcessor::getTransportSnapshot() const noexcept
{
    TransportSnapshot snapshot;
    snapshot.positionSeconds = static_cast<double>(positionMicros.load(std::memory_order_relaxed)) / 1000000.0;
    snapshot.hasPosition = hasPosition.load(std::memory_order_relaxed);
    snapshot.isPlaying = isPlaying.load(std::memory_order_relaxed);
    snapshot.isLooping = isLooping.load(std::memory_order_relaxed);
    return snapshot;
}

voclyrics::SetlistModel VocLyricsAudioProcessor::getSetlistSnapshot() const
{
    const juce::ScopedLock lock(modelLock);
    return setlist;
}

int VocLyricsAudioProcessor::getActiveIndex() const
{
    const juce::ScopedLock lock(modelLock);
    return setlist.activeIndex();
}

juce::String VocLyricsAudioProcessor::getHostRole() const
{
    const juce::ScopedLock lock(modelLock);
    return hostRole;
}

void VocLyricsAudioProcessor::setHostRole(const juce::String& role)
{
    const juce::ScopedLock lock(modelLock);
    hostRole = role.isNotEmpty() ? role : juce::String("Main Singer");
    updateServerState();
}

void VocLyricsAudioProcessor::setSharedScrollLine(int line)
{
    const auto bounded = juce::jmax(-1, line);
    if (server.scrollController() == 0) server.setScrollLine(bounded);
}

void VocLyricsAudioProcessor::selectSong(int index)
{
    const juce::ScopedLock lock(modelLock);
    if (setlist.setActive(index)) lastAutoAdvancedId.clear();
    updateServerState();
}

int VocLyricsAudioProcessor::addSong()
{
    const juce::ScopedLock lock(modelLock);
    const auto result = setlist.addSong();
    updateServerState();
    return result;
}

bool VocLyricsAudioProcessor::duplicateSong(int index)
{
    const juce::ScopedLock lock(modelLock);
    const auto changed = setlist.duplicateSong(index);
    updateServerState();
    return changed;
}

bool VocLyricsAudioProcessor::deleteSong(int index)
{
    const juce::ScopedLock lock(modelLock);
    const auto changed = setlist.removeSong(index);
    updateServerState();
    return changed;
}

bool VocLyricsAudioProcessor::moveSong(int from, int to)
{
    const juce::ScopedLock lock(modelLock);
    const auto changed = setlist.moveSong(from, to);
    updateServerState();
    return changed;
}

void VocLyricsAudioProcessor::renameSong(int index, const juce::String& title)
{
    const juce::ScopedLock lock(modelLock);
    setlist.renameSong(index, title.toStdString());
    updateServerState();
}

void VocLyricsAudioProcessor::setSongMarker(int index, double seconds)
{
    const juce::ScopedLock lock(modelLock);
    setlist.setStartSeconds(index, seconds);
    updateServerState();
}

void VocLyricsAudioProcessor::setSongAutoAdvance(int index, bool enabled)
{
    const juce::ScopedLock lock(modelLock);
    setlist.setAutoAdvance(index, enabled);
    updateServerState();
}

void VocLyricsAudioProcessor::setLyricsText(const juce::String& text)
{
    const juce::ScopedLock lock(modelLock);
    if (auto* song = setlist.activeSong())
        song->lyrics = voclyrics::LrcParser::parse(text.toStdString());
    updateServerState();
}

juce::String VocLyricsAudioProcessor::getLyricsText() const
{
    const juce::ScopedLock lock(modelLock);
    const auto* song = setlist.activeSong();
    return song != nullptr ? juce::String(voclyrics::LrcParser::serialize(song->lyrics)) : juce::String();
}

bool VocLyricsAudioProcessor::tagNextLine()
{
    if (! hasPosition.load(std::memory_order_relaxed)) return false;
    const juce::ScopedLock lock(modelLock);
    auto* song = setlist.activeSong();
    if (song == nullptr) return false;
    auto time = positionMicros.load(std::memory_order_relaxed) / 1000000.0;
    if (song->startSeconds >= 0.0) time -= song->startSeconds;
    if (time < 0.0) return false;
    voclyrics::LiveTagger tagger(*song);
    const auto index = tagger.nextUntimedLine();
    if (index < 0 || ! tagger.tagNext(time)) return false;
    lastTaggedIndex = index;
    previousTagTime = -1.0;
    updateServerState();
    return true;
}

bool VocLyricsAudioProcessor::undoTag()
{
    const juce::ScopedLock lock(modelLock);
    auto* song = setlist.activeSong();
    if (song == nullptr || lastTaggedIndex < 0 || lastTaggedIndex >= static_cast<int>(song->lyrics.size())) return false;
    song->lyrics[static_cast<std::size_t>(lastTaggedIndex)].timeSeconds = previousTagTime;
    lastTaggedIndex = -1;
    updateServerState();
    return true;
}

void VocLyricsAudioProcessor::jumpToSong(int index)
{
    const juce::ScopedLock lock(modelLock);
    // JUCE exposes transport position to plug-ins, but does not provide a
    // portable transport-seek command. Selecting a song therefore updates the
    // setlist; hosts can optionally provide their own marker jump integration.
    if (! setlist.setActive(index)) return;
    lastAutoAdvancedId.clear();
    updateServerState();
}

void VocLyricsAudioProcessor::timerCallback()
{
    if (isPlaying.load(std::memory_order_relaxed))
    {
        const juce::ScopedLock lock(modelLock);
        auto* song = setlist.activeSong();
        if (song != nullptr && song->autoAdvance && song->startSeconds >= 0.0 && song->id != lastAutoAdvancedId)
        {
            double endTime = -1.0;
            for (const auto& line : song->lyrics)
                if (! line.isSection) endTime = juce::jmax(endTime, line.timeSeconds);
            const auto position = positionMicros.load(std::memory_order_relaxed) / 1000000.0;
            if (endTime >= 0.0 && position >= song->startSeconds + endTime + 2.0)
            {
                lastAutoAdvancedId = song->id;
                setlist.skip(1);
            }
        }
    }
    updateServerState();
}

void VocLyricsAudioProcessor::updateServerState()
{
    const juce::ScopedLock lock(modelLock);
    voclyrics::RemoteState remote;
    const auto* song = setlist.activeSong();
    if (song != nullptr)
    {
        remote.songTitle = song->title;
        remote.lyricsLrc = voclyrics::LrcParser::serialize(song->lyrics);
        const auto next = setlist.activeIndex() + 1;
        if (next < static_cast<int>(setlist.songs().size()))
            remote.nextSongTitle = setlist.songs()[static_cast<std::size_t>(next)].title;
    }
    remote.positionSeconds = positionMicros.load(std::memory_order_relaxed) / 1000000.0;
    if (song != nullptr && song->startSeconds >= 0.0)
        remote.positionSeconds -= song->startSeconds;
    remote.hostTimeMilliseconds = juce::Time::getMillisecondCounterHiRes();
    remote.playing = isPlaying.load(std::memory_order_relaxed);
    remote.looping = isLooping.load(std::memory_order_relaxed);
    remote.hostRole = hostRole.toStdString();
    server.setState(std::move(remote));
}

juce::AudioProcessorEditor* VocLyricsAudioProcessor::createEditor()
{
    return new VocLyricsAudioProcessorEditor(*this);
}

void VocLyricsAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    const juce::ScopedLock lock(modelLock);
    juce::ValueTree root("VocLyricsState");
    root.setProperty("active", setlist.activeIndex(), nullptr);
    root.setProperty("port", server.port() > 0 ? server.port() : preferredPort, nullptr);
    root.setProperty("transposeSemitones", getTransposeSemitones(), nullptr);
    root.setProperty("hostRole", hostRole, nullptr);
    for (const auto& song : setlist.songs())
    {
        juce::ValueTree child("Song");
        child.setProperty("id", juce::String(song.id.c_str()), nullptr);
        child.setProperty("title", juce::String(song.title.c_str()), nullptr);
        child.setProperty("lyrics", juce::String(voclyrics::LrcParser::serialize(song.lyrics)), nullptr);
        child.setProperty("start", song.startSeconds, nullptr);
        child.setProperty("autoAdvance", song.autoAdvance, nullptr);
        root.addChild(child, -1, nullptr);
    }
    if (auto xml = root.createXml())
        copyXmlToBinary(*xml, destination);
}

void VocLyricsAudioProcessor::setStateInformation(const void* data, int size)
{
    auto xml = getXmlFromBinary(data, size);
    if (xml == nullptr || ! xml->hasTagName("VocLyricsState")) return;
    const auto root = juce::ValueTree::fromXml(*xml);
    std::vector<voclyrics::Song> songs;
    for (int i = 0; i < root.getNumChildren(); ++i)
    {
        const auto child = root.getChild(i);
        if (! child.hasType("Song")) continue;
        voclyrics::Song song;
        song.id = child.getProperty("id").toString().toStdString();
        song.title = child.getProperty("title", "Untitled Song").toString().toStdString();
        song.lyrics = voclyrics::LrcParser::parse(child.getProperty("lyrics").toString().toStdString());
        song.startSeconds = static_cast<double>(child.getProperty("start", -1.0));
        song.autoAdvance = static_cast<bool>(child.getProperty("autoAdvance", false));
        songs.push_back(std::move(song));
    }
    const juce::ScopedLock lock(modelLock);
    const auto newPort = static_cast<int>(root.getProperty("port", 8080));
    if (newPort != preferredPort && newPort >= 1024 && newPort <= 65535)
    {
        preferredPort = newPort;
        server.stop();
        server.start(preferredPort);
    }
    setlist.replace(std::move(songs), static_cast<int>(root.getProperty("active", 0)));
    setTransposeSemitones(static_cast<int>(root.getProperty("transposeSemitones", 0)));
    hostRole = root.getProperty("hostRole", "Main Singer").toString();
    updateServerState();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocLyricsAudioProcessor();
}
