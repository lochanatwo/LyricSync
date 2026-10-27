#include "ServerManager.h"
#include "NetworkUtils.h"
#include <VocLyricsWebData.h>
#include <juce_core/juce_core.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <limits>
#include <sstream>

namespace voclyrics
{
struct ServerManager::Client
{
    int id = 0;
    std::string name = "Remote device";
    std::string address;
    mg_connection* connection = nullptr;
    std::mutex queueMutex;
    std::condition_variable queueReady;
    std::deque<std::string> queue;
    std::deque<std::string> controls;
    std::atomic<bool> closed { false };
    std::atomic<bool> closeRequested { false };
    std::thread sender;
};

namespace
{
void sendLoop(const std::shared_ptr<ServerManager::Client>& client)
{
    while (! client->closed.load())
    {
        std::string payload;
        {
            std::unique_lock<std::mutex> lock(client->queueMutex);
            client->queueReady.wait_for(lock, std::chrono::milliseconds(500), [&]
            {
                return client->closed.load() || client->closeRequested.load()
                    || ! client->controls.empty() || ! client->queue.empty();
            });
            if (client->closed.load()) return;
            if (client->closeRequested.exchange(false))
            {
                mg_websocket_write(client->connection, MG_WEBSOCKET_OPCODE_CONNECTION_CLOSE, nullptr, 0);
                return;
            }
            if (! client->controls.empty())
            {
                payload = std::move(client->controls.front());
                client->controls.pop_front();
            }
            else
            {
                if (client->queue.empty()) continue;
                payload = std::move(client->queue.back());
                client->queue.clear();
            }
        }
        if (mg_websocket_write(client->connection, MG_WEBSOCKET_OPCODE_TEXT,
                               payload.data(), payload.size()) <= 0)
        {
            client->closed.store(true);
            return;
        }
    }
}

std::string getJsonString(const std::string& json, const std::string& key)
{
    const auto marker = std::string("\"") + key + "\":\"";
    const auto start = json.find(marker);
    if (start == std::string::npos) return {};
    const auto valueStart = start + marker.size();
    const auto end = json.find('"', valueStart);
    return end == std::string::npos ? std::string() : json.substr(valueStart, end - valueStart);
}
}

ServerManager::ServerManager() = default;
ServerManager::~ServerManager() { stop(); }

bool ServerManager::start(int preferredPort)
{
    if (context != nullptr) return true;
    static std::once_flag libraryInit;
    std::call_once(libraryInit, [] { mg_init_library(MG_FEATURES_WEBSOCKET); });
    const char* options[] = {"listening_ports", nullptr, "num_threads", "12",
                             "request_timeout_ms", "3000", "websocket_timeout_ms", "60000", nullptr};
    for (int candidate = std::max(1024, preferredPort); candidate < std::max(1025, preferredPort) + 100; ++candidate)
    {
        const auto listener = std::string("0.0.0.0:") + std::to_string(candidate);
        options[1] = listener.c_str();
        context = mg_start(nullptr, this, options);
        if (context != nullptr)
        {
            boundPort.store(candidate);
            mg_set_request_handler(context, "/", &ServerManager::serveIndex, this);
            mg_set_request_handler(context, "/index.html", &ServerManager::serveIndex, this);
            mg_set_request_handler(context, "/app.js", &ServerManager::serveScript, this);
            mg_set_request_handler(context, "/key-logic.js", &ServerManager::serveKeyLogic, this);
            mg_set_request_handler(context, "/style.css", &ServerManager::serveStyle, this);
            mg_set_request_handler(context, "/manifest.json", &ServerManager::serveManifest, this);
            mg_set_request_handler(context, "/state", &ServerManager::serveState, this);
            mg_set_request_handler(context, "/capacity", &ServerManager::serveCapacity, this);
            mg_set_websocket_handler(context, "/ws", &ServerManager::websocketConnect,
                                     &ServerManager::websocketReady, &ServerManager::websocketData,
                                     &ServerManager::websocketClose, this);
            stopping.store(false);
            broadcaster = std::thread(&ServerManager::broadcastLoop, this);
            return true;
        }
    }
    boundPort.store(0);
    return false;
}

void ServerManager::stop()
{
    stopping.store(true);
    std::vector<std::shared_ptr<Client>> beforeStop;
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        beforeStop = clients;
        beforeStop.insert(beforeStop.end(), retired.begin(), retired.end());
    }
    for (const auto& client : beforeStop)
    {
        client->closed.store(true);
        client->queueReady.notify_all();
    }
    stateChanged.notify_all();
    if (context != nullptr)
    {
        mg_stop(context);
        context = nullptr;
    }
    if (broadcaster.joinable()) broadcaster.join();
    std::vector<std::shared_ptr<Client>> all;
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        all = clients;
        all.insert(all.end(), retired.begin(), retired.end());
        clients.clear();
        retired.clear();
        pendingConnections.clear();
    }
    for (const auto& client : all)
    {
        client->closed.store(true);
        client->queueReady.notify_all();
    }
    for (const auto& client : all)
        if (client->sender.joinable()) client->sender.join();
    boundPort.store(0);
}

void ServerManager::setState(RemoteState next)
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        next.scrollControllerId = activeScrollController;
        next.secondaryScrollControllerId = activeSecondaryScrollController;
        next.scrollLine = activeScrollLine;
        state = std::move(next);
        latestPayload = makePayload(state);
        ++stateRevision;
    }
    stateChanged.notify_one();
}

std::vector<RemoteDevice> ServerManager::devices() const
{
    std::lock_guard<std::mutex> lock(clientsMutex);
    std::vector<RemoteDevice> result;
    result.reserve(clients.size());
    for (const auto& client : clients)
        result.push_back({client->id, client->name, client->address});
    return result;
}
int ServerManager::clientCount() const
{
    std::lock_guard<std::mutex> lock(clientsMutex);
    return static_cast<int>(clients.size());
}
void ServerManager::setScrollController(int id)
{
    setScrollControllers(id, -1);
}
void ServerManager::setScrollControllers(int primaryId, int secondaryId)
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        activeScrollController = std::max(0, primaryId);
        activeSecondaryScrollController = secondaryId < 0 || secondaryId == activeScrollController
            ? -1 : std::max(0, secondaryId);
        activeScrollLine = -1;
        state.scrollControllerId = activeScrollController;
        state.secondaryScrollControllerId = activeSecondaryScrollController;
        state.scrollLine = activeScrollLine;
        latestPayload = makePayload(state);
        ++stateRevision;
    }
    stateChanged.notify_one();
}
int ServerManager::scrollController() const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return activeScrollController;
}
int ServerManager::secondaryScrollController() const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return activeSecondaryScrollController;
}
bool ServerManager::isScrollController(int id) const
{
    return id > 0;
}
bool ServerManager::hostControlsScroll() const
{
    return true;
}
int ServerManager::scrollLine() const
{
    std::lock_guard<std::mutex> lock(stateMutex);
    return activeScrollLine;
}
void ServerManager::setScrollLine(int line)
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        const auto bounded = std::max(-1, line);
        if (activeScrollLine == bounded) return;
        activeScrollLine = bounded;
        state.scrollLine = activeScrollLine;
        latestPayload = makePayload(state);
        ++stateRevision;
    }
    stateChanged.notify_one();
}
void ServerManager::resetScrollLine()
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (activeScrollLine < 0) return;
        activeScrollLine = -1;
        state.scrollLine = -1;
        latestPayload = makePayload(state);
        ++stateRevision;
    }
    stateChanged.notify_one();
}
void ServerManager::disconnectDevice(int id)
{
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (const auto& client : clients)
        if (client->id == id)
        {
            client->closeRequested.store(true);
            client->queueReady.notify_all();
            return;
        }
}
void ServerManager::nudgeDevice(int id, int deltaMilliseconds)
{
    std::lock_guard<std::mutex> lock(clientsMutex);
    for (const auto& client : clients)
        if (client->id == id)
        {
            const auto command = deltaMilliseconds == std::numeric_limits<int>::min()
                ? std::string("{\"type\":\"reset\"}")
                : std::string("{\"type\":\"nudge\",\"delta\":") + std::to_string(deltaMilliseconds) + "}";
            enqueueControl(client, command);
            return;
        }
}

int ServerManager::serveIndex(mg_connection* connection, void*)
{
    return sendAsset(connection, "text/html; charset=utf-8", VocLyricsWeb::index_html, VocLyricsWeb::index_htmlSize);
}
int ServerManager::serveScript(mg_connection* connection, void*)
{
    return sendAsset(connection, "text/javascript; charset=utf-8", VocLyricsWeb::app_js, VocLyricsWeb::app_jsSize);
}
int ServerManager::serveKeyLogic(mg_connection* connection, void*)
{
    return sendAsset(connection, "text/javascript; charset=utf-8", VocLyricsWeb::keylogic_js, VocLyricsWeb::keylogic_jsSize);
}
int ServerManager::serveStyle(mg_connection* connection, void*)
{
    return sendAsset(connection, "text/css; charset=utf-8", VocLyricsWeb::style_css, VocLyricsWeb::style_cssSize);
}
int ServerManager::serveManifest(mg_connection* connection, void*)
{
    return sendAsset(connection, "application/manifest+json; charset=utf-8",
                     VocLyricsWeb::manifest_json, VocLyricsWeb::manifest_jsonSize);
}
int ServerManager::serveState(mg_connection* connection, void* data)
{
    auto* self = static_cast<ServerManager*>(data);
    std::string payload;
    {
        std::lock_guard<std::mutex> lock(self->stateMutex);
        payload = self->latestPayload.empty() ? makePayload(self->state) : self->latestPayload;
    }
    return sendAsset(connection, "application/json; charset=utf-8", payload.data(),
                     static_cast<int>(payload.size()));
}
int ServerManager::serveCapacity(mg_connection* connection, void* data)
{
    const auto* self = static_cast<ServerManager*>(data);
    const auto body = self->clientCount() >= 8 ? "full" : "available";
    return sendAsset(connection, "text/plain; charset=utf-8", body,
                     static_cast<int>(std::char_traits<char>::length(body)));
}
int ServerManager::sendAsset(mg_connection* connection, const char* mime, const void* bytes, int size)
{
    mg_printf(connection, "HTTP/1.1 200 OK\r\nContent-Type: %s\r\nContent-Length: %d\r\n"
                          "Cache-Control: no-store, no-cache, must-revalidate, max-age=0\r\n"
                          "Pragma: no-cache\r\nExpires: 0\r\nConnection: close\r\n\r\n",
              mime, size);
    mg_write(connection, bytes, static_cast<std::size_t>(size));
    return 1;
}

int ServerManager::websocketConnect(const mg_connection* connection, void* data)
{
    auto* self = static_cast<ServerManager*>(data);
    std::lock_guard<std::mutex> lock(self->clientsMutex);
    if (self->clients.size() + self->pendingConnections.size() >= 8)
        return 1;
    self->pendingConnections.insert(connection);
    return 0;
}
void ServerManager::websocketReady(mg_connection* connection, void* data)
{
    auto* self = static_cast<ServerManager*>(data);
    auto client = std::make_shared<Client>();
    client->id = self->nextClientId.fetch_add(1);
    client->connection = connection;
    client->address = addressFor(connection);
    {
        std::lock_guard<std::mutex> lock(self->clientsMutex);
        self->pendingConnections.erase(connection);
        self->clients.push_back(client);
    }
    client->sender = std::thread(sendLoop, client);
    self->enqueueControl(client, std::string("{\"type\":\"hello\",\"clientId\":")
                                      + std::to_string(client->id) + "}");
    std::string payload;
    {
        std::lock_guard<std::mutex> lock(self->stateMutex);
        payload = self->latestPayload.empty() ? makePayload(self->state) : self->latestPayload;
    }
    self->enqueueLatest(client, payload);
}
int ServerManager::websocketData(mg_connection* connection, int bits, char* bytes, std::size_t size, void* data)
{
    auto* self = static_cast<ServerManager*>(data);
    if ((bits & 0x0f) != MG_WEBSOCKET_OPCODE_TEXT || bytes == nullptr || size == 0)
        return 1;
    const std::string message(bytes, size);
    const auto name = getJsonString(message, "name");
    if (message.find("\"type\":\"ping\"") != std::string::npos)
    {
        const auto key = message.find("\"clientSent\":");
        double sent = 0.0;
        if (key != std::string::npos)
        {
            try { sent = std::stod(message.substr(key + 13)); } catch (...) {}
        }
        std::lock_guard<std::mutex> lock(self->clientsMutex);
        for (const auto& client : self->clients)
            if (client->connection == connection)
            {
                std::ostringstream out;
                out << "{\"type\":\"pong\",\"clientSent\":" << sent
                    << ",\"hostMs\":" << juce::Time::getMillisecondCounterHiRes() << '}';
                self->enqueueControl(client, out.str());
                break;
            }
    }
    else if (! name.empty())
    {
        std::lock_guard<std::mutex> lock(self->clientsMutex);
        for (const auto& client : self->clients)
            if (client->connection == connection)
                client->name = name.substr(0, 48);
    }
    else if (message.find("\"type\":\"scroll\"") != std::string::npos)
    {
        const auto key = message.find("\"line\":");
        if (key != std::string::npos)
        {
            try
            {
                const auto line = std::max(-1, std::stoi(message.substr(key + 7)));
                int id = 0;
                {
                    std::lock_guard<std::mutex> lock(self->clientsMutex);
                    for (const auto& client : self->clients)
                        if (client->connection == connection) { id = client->id; break; }
                }
                if (id > 0 && self->isScrollController(id))
                {
                    {
                        std::lock_guard<std::mutex> lock(self->stateMutex);
                        self->activeScrollLine = line;
                        self->state.scrollLine = line;
                        self->latestPayload = makePayload(self->state);
                        ++self->stateRevision;
                    }
                    self->stateChanged.notify_one();
                }
            }
            catch (...) {}
        }
    }
    return 1;
}
void ServerManager::websocketClose(const mg_connection* connection, void* data)
{
    auto* self = static_cast<ServerManager*>(data);
    {
        std::lock_guard<std::mutex> lock(self->clientsMutex);
        const auto found = std::find_if(self->clients.begin(), self->clients.end(), [connection](const auto& client)
        {
            return client->connection == connection;
        });
        if (found != self->clients.end())
        {
            const auto id = (*found)->id;
            (*found)->closed.store(true);
            (*found)->queueReady.notify_all();
            self->retired.push_back(*found);
            self->clients.erase(found);
            const auto primary = self->scrollController();
            const auto secondary = self->secondaryScrollController();
            if (primary == id)
                self->setScrollControllers(secondary >= 0 ? secondary : 0, -1);
            else if (secondary == id)
                self->setScrollControllers(primary, -1);
        }
        else self->pendingConnections.erase(connection);
    }
}

std::string ServerManager::makePayload(const RemoteState& value)
{
    std::ostringstream out;
    out << "{\"type\":\"state\",\"title\":\"" << escapeJson(value.songTitle)
        << "\",\"songId\":\"" << escapeJson(value.songId)
        << "\",\"next\":\"" << escapeJson(value.nextSongTitle)
        << "\",\"lyrics\":\"" << escapeJson(value.lyricsLrc)
        << "\",\"position\":" << value.positionSeconds
        << ",\"hostMs\":" << value.hostTimeMilliseconds
        << ",\"playing\":" << (value.playing ? "true" : "false")
        << ",\"looping\":" << (value.looping ? "true" : "false")
        << ",\"controllerId\":" << value.scrollControllerId
        << ",\"controllerId2\":" << value.secondaryScrollControllerId
        << ",\"scrollLine\":" << value.scrollLine
        << ",\"hostRole\":\"" << escapeJson(value.hostRole)
        << "\",\"songKey\":" << value.songKeyPitchClass
        << ",\"activeLine\":" << value.activeLine << '}';
    return out.str();
}
std::string ServerManager::escapeJson(const std::string& value)
{
    std::string result;
    result.reserve(value.size());
    for (const auto character : value)
    {
        switch (character)
        {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: if (static_cast<unsigned char>(character) >= 0x20) result += character;
        }
    }
    return result;
}
std::string ServerManager::addressFor(const mg_connection* connection)
{
    const auto* info = mg_get_request_info(connection);
    if (info == nullptr) return "LAN device";
    return info->remote_addr[0] != '\0' ? info->remote_addr : "LAN device";
}
void ServerManager::enqueueLatest(const std::shared_ptr<Client>& client, const std::string& payload)
{
    std::lock_guard<std::mutex> lock(client->queueMutex);
    client->queue.clear();
    client->queue.push_back(payload);
    client->queueReady.notify_one();
}
void ServerManager::enqueueControl(const std::shared_ptr<Client>& client, const std::string& payload)
{
    std::lock_guard<std::mutex> lock(client->queueMutex);
    if (client->controls.size() >= 8) client->controls.pop_front();
    client->controls.push_back(payload);
    client->queueReady.notify_one();
}
void ServerManager::retireClosedClients()
{
    std::vector<std::shared_ptr<Client>> finished;
    {
        std::lock_guard<std::mutex> lock(clientsMutex);
        for (auto it = retired.begin(); it != retired.end();)
        {
            if ((*it)->closed.load())
            {
                finished.push_back(*it);
                it = retired.erase(it);
            }
            else ++it;
        }
    }
    for (const auto& client : finished)
        if (client->sender.joinable()) client->sender.join();
}
void ServerManager::broadcastLoop()
{
    std::uint64_t lastRevision = 0;
    while (! stopping.load())
    {
        RemoteState current;
        std::uint64_t currentRevision = 0;
        {
            std::unique_lock<std::mutex> lock(stateMutex);
            stateChanged.wait_for(lock, std::chrono::milliseconds(40), [this, lastRevision]
            {
                return stopping.load() || stateRevision != lastRevision;
            });
            if (stopping.load()) break;
            current = state;
            currentRevision = stateRevision;
        }
        const auto payload = makePayload(current);
        std::vector<std::shared_ptr<Client>> connected;
        {
            std::lock_guard<std::mutex> lock(clientsMutex);
            connected = clients;
        }
        for (const auto& client : connected)
            if (! client->closed.load()) enqueueLatest(client, payload);
        retireClosedClients();
        lastRevision = currentRevision;
    }
}
}
