#pragma once

#include "SyncClock.h"
#include <civetweb.h>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace voclyrics
{
struct RemoteDevice
{
    int id = 0;
    std::string name;
    std::string ipAddress;
};

struct RemoteState
{
    std::string songTitle;
    std::string nextSongTitle;
    std::string lyricsLrc;
    double positionSeconds = 0.0;
    double hostTimeMilliseconds = 0.0;
    bool playing = false;
    bool looping = false;
    int scrollLine = -1;
    int scrollControllerId = 0;
    int secondaryScrollControllerId = -1;
    std::string hostRole;
    int activeLine = -1;
    std::string songId;
    int songKeyPitchClass = -1;
};

class ServerManager
{
public:
    struct Client;
    ServerManager();
    ~ServerManager();
    bool start(int preferredPort = 8080);
    void stop();
    bool isRunning() const noexcept { return context != nullptr; }
    int port() const noexcept { return boundPort.load(); }
    void setState(RemoteState state);
    std::vector<RemoteDevice> devices() const;
    void disconnectDevice(int id);
    void nudgeDevice(int id, int deltaMilliseconds);
    int clientCount() const;
    void setScrollController(int id);
    void setScrollControllers(int primaryId, int secondaryId);
    int scrollController() const;
    int secondaryScrollController() const;
    bool isScrollController(int id) const;
    bool hostControlsScroll() const;
    int scrollLine() const;
    void setScrollLine(int line);
    void resetScrollLine();

private:
    static int serveIndex(mg_connection*, void*);
    static int serveScript(mg_connection*, void*);
    static int serveKeyLogic(mg_connection*, void*);
    static int serveStyle(mg_connection*, void*);
    static int serveManifest(mg_connection*, void*);
    static int serveState(mg_connection*, void*);
    static int serveCapacity(mg_connection*, void*);
    static int websocketConnect(const mg_connection*, void*);
    static void websocketReady(mg_connection*, void*);
    static int websocketData(mg_connection*, int, char*, std::size_t, void*);
    static void websocketClose(const mg_connection*, void*);
    static int sendAsset(mg_connection*, const char*, const void*, int);
    static std::string makePayload(const RemoteState&);
    static std::string escapeJson(const std::string&);
    static std::string addressFor(const mg_connection*);
    void broadcastLoop();
    void retireClosedClients();
    void enqueueLatest(const std::shared_ptr<Client>&, const std::string&);
    void enqueueControl(const std::shared_ptr<Client>&, const std::string&);

    mutable std::mutex stateMutex;
    std::condition_variable stateChanged;
    RemoteState state;
    std::string latestPayload;
    std::uint64_t stateRevision = 0;
    int activeScrollController = 0;
    int activeSecondaryScrollController = -1;
    int activeScrollLine = -1;
    mutable std::mutex clientsMutex;
    std::vector<std::shared_ptr<Client>> clients;
    std::vector<std::shared_ptr<Client>> retired;
    std::unordered_set<const mg_connection*> pendingConnections;
    std::atomic<int> boundPort { 0 };
    std::atomic<int> nextClientId { 1 };
    std::atomic<bool> stopping { false };
    std::thread broadcaster;
    mg_context* context = nullptr;
};
}
