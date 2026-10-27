#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Lyrics/ChordTransposer.h"
#include "Lyrics/LiveTagger.h"

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

std::vector<int> displayLineStarts(const voclyrics::Song& song)
{
    std::vector<int> starts;
    for (int i = 0; i < static_cast<int>(song.lyrics.size()); ++i)
    {
        if (song.lyrics[static_cast<std::size_t>(i)].isSection
            || voclyrics::isSongKeyLine(song.lyrics[static_cast<std::size_t>(i)].text)) continue;
        if (isDividerLine(song.lyrics[static_cast<std::size_t>(i)].text)) continue;
        starts.push_back(i);
        if (voclyrics::isChordOnlyLine(song.lyrics[static_cast<std::size_t>(i)].text)
            && i + 1 < static_cast<int>(song.lyrics.size())
            && ! song.lyrics[static_cast<std::size_t>(i + 1)].isSection)
            ++i;
    }
    return starts;
}
}

VocLyricsAudioProcessor::VocLyricsAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true))
{
    server.start(preferredPort);
    startTimerHz(20);
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

void VocLyricsAudioProcessor::setTargetKeyForActiveSong(int targetPitchClass)
{
    if (targetPitchClass < 0 || targetPitchClass >= 12) return;
    const juce::ScopedLock lock(modelLock);
    const auto* song = setlist.activeSong();
    if (song == nullptr) return;
    const auto sourcePitch = voclyrics::songKeyPitchClass(*song);
    if (sourcePitch < 0) return;
    targetKeyBySong[song->id] = targetPitchClass;
    setTransposeSemitones(voclyrics::transposeSemitonesForKeys(sourcePitch, targetPitchClass));
}

void VocLyricsAudioProcessor::setSourceKeyForActiveSong(int sourcePitchClass)
{
    if (sourcePitchClass < -1 || sourcePitchClass >= 12) return;
    const juce::ScopedLock lock(modelLock);
    auto* song = setlist.activeSong();
    if (song == nullptr) return;
    song->sourceKeyOverride = sourcePitchClass;
    applyActiveSongTranspose();
    updateServerState();
}

void VocLyricsAudioProcessor::applyActiveSongTranspose()
{
    const auto* song = setlist.activeSong();
    if (song == nullptr) { setTransposeSemitones(0); return; }
    const auto found = targetKeyBySong.find(song->id);
    const auto sourcePitch = voclyrics::songKeyPitchClass(*song);
    setTransposeSemitones(found != targetKeyBySong.end() && sourcePitch >= 0
        ? voclyrics::transposeSemitonesForKeys(sourcePitch, found->second) : 0);
}

void VocLyricsAudioProcessor::setSharedScrollLine(int line)
{
    const auto bounded = juce::jmax(-1, line);
    if (server.hostControlsScroll()) server.setScrollLine(bounded);
}

void VocLyricsAudioProcessor::moveSharedScrollLine(int direction)
{
    if (! server.hostControlsScroll()) return;
    const juce::ScopedLock lock(modelLock);
    const auto* song = setlist.activeSong();
    if (song == nullptr || song->lyrics.empty()) return;
    const auto starts = displayLineStarts(*song);
    if (starts.empty()) return;
    const auto current = server.scrollLine();
    auto position = 0;
    for (int i = 0; i < static_cast<int>(starts.size()); ++i)
        if (starts[static_cast<std::size_t>(i)] <= current) position = i;
    const auto step = direction < 0 ? juce::jmin(-1, direction) : juce::jmax(1, direction);
    position = juce::jlimit(0, static_cast<int>(starts.size()) - 1, position + step);
    server.setScrollLine(starts[static_cast<std::size_t>(position)]);
    if (autoScrollEnabled.load(std::memory_order_relaxed))
        autoScrollLastTickMilliseconds = juce::Time::getMillisecondCounterHiRes();
    if (tapScrollEnabled.load(std::memory_order_relaxed))
        tapScrollLastTickMilliseconds = juce::Time::getMillisecondCounterHiRes();
}

void VocLyricsAudioProcessor::setSharedScrollController(int id)
{
    setSharedScrollControllers(id, -1);
}

void VocLyricsAudioProcessor::setSharedScrollControllers(int primaryId, int secondaryId)
{
    const auto primary = juce::jmax(0, primaryId);
    const auto secondary = secondaryId < 0 ? -1 : juce::jmax(0, secondaryId);
    if (primary != 0 || secondary > 0)
    {
        setAutoScrollEnabled(false);
        stopTapTempo();
    }
    server.setScrollControllers(primary, secondary);
}

void VocLyricsAudioProcessor::setAutoScrollEnabled(bool enabled)
{
    if (! enabled)
    {
        autoScrollEnabled.store(false, std::memory_order_relaxed);
        autoScrollLastTickMilliseconds = 0.0;
        return;
    }

    stopTapTempo();
    if (! server.hostControlsScroll()) server.setScrollControllers(0, -1);
    const juce::ScopedLock lock(modelLock);
    const auto* song = setlist.activeSong();
    if (song == nullptr)
    {
        autoScrollEnabled.store(false, std::memory_order_relaxed);
        return;
    }
    const auto starts = displayLineStarts(*song);
    if (starts.empty())
    {
        autoScrollEnabled.store(false, std::memory_order_relaxed);
        return;
    }
    if (server.scrollLine() < 0) server.setScrollLine(starts.front());
    autoScrollLastTickMilliseconds = juce::Time::getMillisecondCounterHiRes();
    autoScrollEnabled.store(true, std::memory_order_relaxed);
}

void VocLyricsAudioProcessor::setAutoScrollLinesPerMinute(int linesPerMinute)
{
    autoScrollLinesPerMinute.store(juce::jlimit(1, 60, linesPerMinute), std::memory_order_relaxed);
    autoScrollLastTickMilliseconds = juce::Time::getMillisecondCounterHiRes();
}

void VocLyricsAudioProcessor::registerTapTempo()
{
    if (! server.hostControlsScroll()) return;
    const juce::ScopedLock lock(modelLock);
    const auto* song = setlist.activeSong();
    if (song == nullptr) return;
    const auto starts = displayLineStarts(*song);
    if (starts.empty()) return;
    if (! tapScrollEnabled.load(std::memory_order_relaxed)) tapTempoClock.reset();
    setAutoScrollEnabled(false);
    const auto now = juce::Time::getMillisecondCounterHiRes();
    const auto bpm = tapTempoClock.tap(now);
    if (tapTempoClock.tapCount() == 1)
        tapScrollLastTickMilliseconds = 0.0;
    tapTempoBpm.store(bpm, std::memory_order_relaxed);
    if (server.scrollLine() < 0) server.setScrollLine(starts.front());
    if (bpm > 0 && tapScrollLastTickMilliseconds <= 0.0)
        tapScrollLastTickMilliseconds = now;
    tapScrollEnabled.store(true, std::memory_order_relaxed);
}

void VocLyricsAudioProcessor::setManualTempoBpm(int bpm)
{
    if (! server.hostControlsScroll()) return;
    const juce::ScopedLock lock(modelLock);
    const auto* song = setlist.activeSong();
    if (song == nullptr) return;
    const auto starts = displayLineStarts(*song);
    if (starts.empty()) return;
    setAutoScrollEnabled(false);
    tapTempoClock.reset();
    const auto bounded = juce::jlimit(8, 300, bpm);
    manualTempoBpm.store(bounded, std::memory_order_relaxed);
    tapTempoBpm.store(bounded, std::memory_order_relaxed);
    if (server.scrollLine() < 0) server.setScrollLine(starts.front());
    tapScrollLastTickMilliseconds = juce::Time::getMillisecondCounterHiRes();
    tapScrollEnabled.store(true, std::memory_order_relaxed);
}

void VocLyricsAudioProcessor::stopTapTempo()
{
    tapScrollEnabled.store(false, std::memory_order_relaxed);
    tapTempoBpm.store(0, std::memory_order_relaxed);
    tapScrollLastTickMilliseconds = 0.0;
}

void VocLyricsAudioProcessor::selectSong(int index)
{
    const juce::ScopedLock lock(modelLock);
    if (setlist.setActive(index))
    {
        lastAutoAdvancedId.clear();
        stopTapTempo();
        server.resetScrollLine();
        applyActiveSongTranspose();
    }
    updateServerState();
}

int VocLyricsAudioProcessor::addSong()
{
    const juce::ScopedLock lock(modelLock);
    const auto result = setlist.addSong();
    applyActiveSongTranspose();
    updateServerState();
    return result;
}

bool VocLyricsAudioProcessor::duplicateSong(int index)
{
    const juce::ScopedLock lock(modelLock);
    const auto changed = setlist.duplicateSong(index);
    if (changed) applyActiveSongTranspose();
    updateServerState();
    return changed;
}

bool VocLyricsAudioProcessor::deleteSong(int index)
{
    const juce::ScopedLock lock(modelLock);
    const auto changed = setlist.removeSong(index);
    if (changed) applyActiveSongTranspose();
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
    applyActiveSongTranspose();
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
    stopTapTempo();
    server.resetScrollLine();
    applyActiveSongTranspose();
    updateServerState();
}

void VocLyricsAudioProcessor::timerCallback()
{
    if (autoScrollEnabled.load(std::memory_order_relaxed) && server.hostControlsScroll())
    {
        if (server.scrollLine() < 0)
        {
            setAutoScrollEnabled(true);
        }
        else
        {
            const auto now = juce::Time::getMillisecondCounterHiRes();
            const auto interval = 60000.0 / static_cast<double>(autoScrollLinesPerMinute.load(std::memory_order_relaxed));
            if (autoScrollLastTickMilliseconds <= 0.0) autoScrollLastTickMilliseconds = now;
            if (now - autoScrollLastTickMilliseconds >= interval)
            {
                const auto previousLine = server.scrollLine();
                moveSharedScrollLine(1);
                if (server.scrollLine() == previousLine)
                    setAutoScrollEnabled(false);
                else
                    autoScrollLastTickMilliseconds = now;
            }
        }
    }
    if (tapScrollEnabled.load(std::memory_order_relaxed) && server.hostControlsScroll())
    {
        const auto bpm = tapTempoBpm.load(std::memory_order_relaxed);
        if (bpm > 0)
        {
            const auto now = juce::Time::getMillisecondCounterHiRes();
            const auto interval = voclyrics::TapTempo::fourBeatBarMilliseconds(bpm);
            if (now - tapScrollLastTickMilliseconds >= interval)
            {
                const auto previousLine = server.scrollLine();
                moveSharedScrollLine(1);
                if (server.scrollLine() == previousLine)
                    stopTapTempo();
                else
                    tapScrollLastTickMilliseconds = now;
            }
        }
    }
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
                server.resetScrollLine();
                stopTapTempo();
                applyActiveSongTranspose();
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
        remote.songId = song->id;
        remote.songTitle = song->title;
        remote.lyricsLrc = voclyrics::LrcParser::serialize(song->lyrics);
        remote.songKeyPitchClass = voclyrics::songKeyPitchClass(*song);
        const auto next = setlist.activeIndex() + 1;
        if (next < static_cast<int>(setlist.songs().size()))
            remote.nextSongTitle = setlist.songs()[static_cast<std::size_t>(next)].title;
    }
    remote.positionSeconds = positionMicros.load(std::memory_order_relaxed) / 1000000.0;
    if (song != nullptr && song->startSeconds >= 0.0)
        remote.positionSeconds -= song->startSeconds;
    if (song != nullptr)
        remote.activeLine = voclyrics::activeLineAt(*song, remote.positionSeconds);
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
    root.setProperty("autoScrollLinesPerMinute", getAutoScrollLinesPerMinute(), nullptr);
    root.setProperty("manualTempoBpm", getManualTempoBpm(), nullptr);
    for (const auto& song : setlist.songs())
    {
        juce::ValueTree child("Song");
        child.setProperty("id", juce::String(song.id.c_str()), nullptr);
        child.setProperty("title", juce::String(song.title.c_str()), nullptr);
        child.setProperty("lyrics", juce::String(voclyrics::LrcParser::serialize(song.lyrics)), nullptr);
        child.setProperty("start", song.startSeconds, nullptr);
        child.setProperty("autoAdvance", song.autoAdvance, nullptr);
        child.setProperty("sourceKeyOverride", song.sourceKeyOverride, nullptr);
        const auto target = targetKeyBySong.find(song.id);
        if (target != targetKeyBySong.end())
            child.setProperty("targetKey", target->second, nullptr);
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
    std::unordered_map<std::string, int> restoredTargets;
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
        song.sourceKeyOverride = juce::jlimit(-1, 11, static_cast<int>(child.getProperty("sourceKeyOverride", -1)));
        if (child.hasProperty("targetKey"))
        {
            const auto target = static_cast<int>(child.getProperty("targetKey"));
            if (target >= 0 && target < 12) restoredTargets[song.id] = target;
        }
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
    targetKeyBySong = std::move(restoredTargets);
    if (targetKeyBySong.empty())
    {
        const auto* activeSong = setlist.activeSong();
        const auto sourcePitch = activeSong != nullptr ? voclyrics::songKeyPitchClass(*activeSong) : -1;
        if (sourcePitch >= 0)
        {
            const auto legacyShift = static_cast<int>(root.getProperty("transposeSemitones", 0));
            targetKeyBySong[activeSong->id] = (sourcePitch + legacyShift + 120) % 12;
        }
    }
    applyActiveSongTranspose();
    setAutoScrollLinesPerMinute(static_cast<int>(root.getProperty("autoScrollLinesPerMinute", 12)));
    manualTempoBpm.store(juce::jlimit(8, 300, static_cast<int>(root.getProperty("manualTempoBpm", 120))),
                         std::memory_order_relaxed);
    setAutoScrollEnabled(false);
    stopTapTempo();
    hostRole = root.getProperty("hostRole", "Main Singer").toString();
    updateServerState();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VocLyricsAudioProcessor();
}
