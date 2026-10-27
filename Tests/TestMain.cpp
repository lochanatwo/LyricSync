#include "Lyrics/LrcParser.h"
#include "Lyrics/ChordTransposer.h"
#include "Lyrics/LiveTagger.h"
#include "Lyrics/SetlistModel.h"
#include "Lyrics/TapTempo.h"
#include "Network/ServerManager.h"
#include "Network/SyncClock.h"
#include <civetweb.h>

#include <array>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace
{
void expect(bool condition, const char* message)
{
    if (! condition) throw std::runtime_error(message);
}

void testLrcParser()
{
    const auto parsed = voclyrics::LrcParser::parse("[00:01.20]first\n[00:02.00][00:03.5]twice\nplain line\n[Chorus]\n");
    expect(parsed.size() == 5, "LRC parser should retain timed, duplicate-time, plain and section rows");
    expect(std::abs(parsed[0].timeSeconds - 1.2) < 0.001, "LRC timestamp conversion failed");
    expect(parsed[1].text == "twice" && std::abs(parsed[2].timeSeconds - 3.5) < 0.001,
           "Multiple LRC timestamps failed");
    expect(parsed[3].timeSeconds < 0.0 && parsed[3].text == "plain line", "Plain line parsing failed");
    expect(parsed[4].isSection && parsed[4].text == "Chorus", "Section tag parsing failed");
    const auto serialized = voclyrics::LrcParser::serialize(parsed);
    expect(serialized.find("[00:01.20]first") != std::string::npos, "LRC serialization failed");
}

void testLiveTaggerAndSetlist()
{
    voclyrics::Song song;
    song.lyrics = voclyrics::LrcParser::parse("Key- A#\n[00:01.00]timed\nuntagged one\nuntagged two\n");
    expect(voclyrics::songKeyPitchClass(song) == 10, "Song key line should recognize A#");
    expect(voclyrics::transposeSemitonesForKeys(10, 11) == 1, "Target key should produce an integer semitone shift");
    expect(voclyrics::keyPitchClass("Key- A# | TRANSPOSED BY: 0") == 10,
           "Key metadata beside transpose amount must keep key selection available");
    expect(voclyrics::keyPitchClass("Key- Bb (TRANSPOSED BY: -2)") == 10,
           "Flat key metadata beside transpose amount must be recognized");
    expect(voclyrics::keyPitchClass("\xef\xbb\xbfKey- A\xe2\x99\xaf") == 10,
           "UTF-8 file marker and sharp sign should still reveal the source key");
    song.sourceKeyOverride = 9;
    expect(voclyrics::songKeyPitchClass(song) == 9, "Explicit source key should override unrecognized file metadata");
    expect(voclyrics::transposeSemitonesForKeys(voclyrics::songKeyPitchClass(song), 0) == 3,
           "Changing the target from A to C should produce +3 semitones");
    expect(voclyrics::isChordOnlyLine("A     E/G#"), "Chord row must be detected");
    expect(voclyrics::transposeChordLine("A     E/G#", 3) == "C     G/B",
           "Target key must change both main and slash chords on desktop");
    voclyrics::LiveTagger tagger(song);
    expect(tagger.tagNext(12.345), "Live tag should timestamp the next untimed line");
    expect(song.lyrics.front().timeSeconds < 0.0, "Live tagging must skip the song key line");
    expect(tagger.nextUntimedLine() >= 0, "Live tag should advance to the following untimed line");
    expect(tagger.undo(), "Live tag undo failed");
    expect(tagger.nextUntimedLine() >= 0, "Undo should restore an untimed line");
    voclyrics::SetlistModel setlist;
    setlist.addSong("Second");
    expect(setlist.moveSong(1, 0), "Setlist reorder failed");
    expect(setlist.activeSong()->title == "Second", "Setlist active item should track reorder");
}

void testTapTempo()
{
    voclyrics::TapTempo tempo;
    expect(tempo.tap(0.0) == 0, "The first tap should wait for a beat interval");
    expect(tempo.tap(500.0) == 0 && tempo.tap(1000.0) == 0,
           "Tempo should wait for four quarter-note taps");
    expect(tempo.tap(1500.0) == 120, "Four half-second taps should set 120 BPM");
    expect(tempo.tap(3000.0) == 80, "Slow taps should lower the tempo");
    tempo.reset();
    expect(tempo.tap(0.0) == 0 && tempo.tap(1500.0) == 0 && tempo.tap(3000.0) == 0,
           "A slow four-beat count should wait for the fourth tap");
    expect(tempo.tap(4500.0) == 40, "Four 1.5-second taps should allow 40 BPM");
    expect(tempo.tap(14000.0) == 0 && tempo.tapCount() == 1,
           "A long pause should start a fresh four-tap sequence");
}

void testSyncClock()
{
    const voclyrics::SyncFrame playing { 10.0, 1000.0, true };
    expect(std::abs(voclyrics::SyncClock::estimatePosition(playing, 1125.0, 0.0, 25.0) - 10.15) < 0.0001,
           "Playing interpolation or nudge failed");
    auto stopped = playing;
    stopped.playing = false;
    expect(voclyrics::SyncClock::estimatePosition(stopped, 3000.0, 0.0, 0.0) == 10.0,
           "Stopped interpolation should hold position");
}

struct ClientProbe
{
    std::atomic<long long> receivedNanoseconds { 0 };
};

int receiveWebSocket(mg_connection*, int flags, char* data, std::size_t length, void* userData)
{
    if ((flags & 0x0f) == MG_WEBSOCKET_OPCODE_TEXT && data != nullptr)
    {
        const std::string message(data, length);
        if (message.find("\"position\":42") != std::string::npos)
        {
            const auto now = std::chrono::steady_clock::now().time_since_epoch();
            const auto value = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();
            static_cast<ClientProbe*>(userData)->receivedNanoseconds.store(value);
        }
    }
    return 1;
}
void closeWebSocket(const mg_connection*, void*) {}

void testEightWebSocketClients()
{
    voclyrics::ServerManager server;
    expect(server.start(28080), "Embedded HTTP/WebSocket server failed to start");
    server.setScrollControllers(3, 7);
    expect(server.isScrollController(3) && server.isScrollController(7),
           "Both selected remote devices should control shared scrolling");
    expect(! server.hostControlsScroll(), "Host should not control scrolling unless selected");
    server.setScrollLine(4);
    expect(server.scrollLine() < 0, "Unselected host must not change shared scrolling");
    server.setScrollControllers(0, 7);
    expect(server.hostControlsScroll() && server.isScrollController(7),
           "Host and one remote device should be valid simultaneous controllers");
    server.setScrollLine(4);
    expect(server.scrollLine() == 4, "Selected host should change shared scrolling");
    server.resetScrollLine();
    server.setState({"Latency test", "", "[00:00.00]test", 0.0, 1.0, true, false});

    std::array<ClientProbe, 8> probes;
    std::array<mg_connection*, 8> clients {};
    for (std::size_t i = 0; i < clients.size(); ++i)
    {
        char error[256] = {};
        clients[i] = mg_connect_websocket_client("127.0.0.1", server.port(), 0, error, sizeof(error),
                                                   "/ws", nullptr, &receiveWebSocket,
                                                   &closeWebSocket, &probes[i]);
        expect(clients[i] != nullptr, "Could not connect one of eight WebSocket clients");
    }

    const auto connectDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (server.clientCount() < 8 && std::chrono::steady_clock::now() < connectDeadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    expect(server.clientCount() == 8, "Server did not retain eight simultaneous clients");

    const auto started = std::chrono::steady_clock::now();
    server.setState({"Latency test", "", "[00:00.00]test", 42.0, 2.0, true, false});
    const auto deadline = started + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline)
    {
        bool allReceived = true;
        for (const auto& probe : probes)
            allReceived = allReceived && probe.receivedNanoseconds.load() != 0;
        if (allReceived) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    long long latest = 0;
    for (const auto& probe : probes)
    {
        const auto received = probe.receivedNanoseconds.load();
        expect(received != 0, "An active WebSocket client missed a playhead update");
        latest = std::max(latest, received);
    }
    const auto startedNs = std::chrono::duration_cast<std::chrono::nanoseconds>(started.time_since_epoch()).count();
    expect(latest - startedNs < 50000000LL, "Eight-client playhead broadcast exceeded the 50 ms target");

    char error[256] = {};
    ClientProbe ninthProbe;
    auto* ninth = mg_connect_websocket_client("127.0.0.1", server.port(), 0, error, sizeof(error),
                                                "/ws", nullptr, &receiveWebSocket,
                                                &closeWebSocket, &ninthProbe);
    expect(ninth == nullptr, "A ninth WebSocket client should be refused");
    for (auto* client : clients) if (client != nullptr) mg_close_connection(client);
    server.stop();
}
}

int main()
{
    try
    {
        testLrcParser();
        testLiveTaggerAndSetlist();
        testSyncClock();
        testTapTempo();
        testEightWebSocketClients();
        std::cout << "VocLyrics tests passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "VocLyrics test failure: " << error.what() << '\n';
        return 1;
    }
}
