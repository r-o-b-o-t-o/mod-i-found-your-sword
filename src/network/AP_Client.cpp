#include "network/AP_Client.h"
#include "network/AP_WebSocketClient.h"
#include "network/AP_WebSocketService.h"

#include <algorithm>
#include <boost/asio/ssl/error.hpp>
#include <boost/beast/core/error.hpp>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace ModArchipelaWoW::Network
{
    // ---------------------------------------------------------------------------
    // Data package disk cache
    // ---------------------------------------------------------------------------

    static constexpr auto kDataPackageCacheDir = "ap_datapackage_cache";

    /// Checksums come from the server; only accept plain hex digests so they
    /// can safely be used as file names.
    static bool IsValidChecksum(const std::string& checksum)
    {
        if (checksum.empty() || checksum.size() > 64)
        {
            return false;
        }

        return std::all_of(checksum.begin(), checksum.end(),
            [](unsigned char c) { return std::isxdigit(c) != 0; });
    }

    static std::filesystem::path DataPackageCachePath(const std::string& checksum)
    {
        return std::filesystem::path(kDataPackageCacheDir) / (checksum + ".json");
    }

    static nlohmann::json LoadDataPackageCache(const std::string& checksum)
    {
        if (!IsValidChecksum(checksum))
        {
            return nlohmann::json();
        }

        std::ifstream file(DataPackageCachePath(checksum));
        if (!file)
        {
            return nlohmann::json();
        }

        nlohmann::json data = nlohmann::json::parse(file, nullptr, false);
        if (data.is_discarded())
        {
            return nlohmann::json();
        }

        return data;
    }

    static void SaveDataPackageCache(const std::string& checksum, const nlohmann::json& gameData)
    {
        if (!IsValidChecksum(checksum))
        {
            return;
        }

        std::error_code ec;
        std::filesystem::create_directories(kDataPackageCacheDir, ec);
        if (ec)
        {
            return;
        }

        std::ofstream file(DataPackageCachePath(checksum));
        if (file)
        {
            file << gameData.dump();
        }
    }

    // ---------------------------------------------------------------------------
    // EventQueue - thread-safe bridge between the io_context and game threads
    // ---------------------------------------------------------------------------

    struct Client::EventQueue
    {
        enum class Type { Open, Close, Message, Error, TlsHandshakeFailed };

        struct Event
        {
            Type type;
            std::string data;
        };

        std::mutex mutex;
        std::vector<Event> events;

        void Push(Type type, std::string data = {})
        {
            std::lock_guard lock(mutex);
            events.push_back({ type, std::move(data) });
        }

        std::vector<Event> Drain()
        {
            std::lock_guard lock(mutex);
            std::vector<Event> drained;
            drained.swap(events);
            return drained;
        }
    };

    // ---------------------------------------------------------------------------
    // TextNode
    // ---------------------------------------------------------------------------

    Client::TextNode Client::TextNode::FromJson(const json& j)
    {
        TextNode node;
        node.type = j.value("type", "");
        node.color = j.value("color", "");
        node.text = j.value("text", "");
        node.player = j.value("player", 0);
        node.flags = j.value("flags", 0U);
        node.hintStatus = j.value("hint_status", 0U);
        return node;
    }

    // ---------------------------------------------------------------------------
    // Construction / Destruction
    // ---------------------------------------------------------------------------

    Client::Client(WebSocketService& wsService, const std::string& uuid, const std::string& game,
        const std::string& host, const std::string& port)
        : wsService(wsService)
        , uuid(uuid)
        , game(game)
        , host(host)
        , port(port)
        , eventQueue(std::make_shared<EventQueue>())
        , dataPackage({ {"version", -1}, {"games", json::object()} })
    {
        // Accept scheme-prefixed hosts for compatibility with the old
        // URI-based configuration; an explicit ws:// starts in plaintext.
        if (this->host.rfind("wss://", 0) == 0)
        {
            this->host.erase(0, 6);
        }
        else if (this->host.rfind("ws://", 0) == 0)
        {
            this->host.erase(0, 5);
            preferTls = false;
            currentAttemptTls = false;
        }
    }

    Client::~Client()
    {
        if (ws)
        {
            ws->Stop();
        }
        ws.reset();
    }

    // ---------------------------------------------------------------------------
    // Poll - drain events, process, handle reconnection
    // ---------------------------------------------------------------------------

    void Client::Poll()
    {
        auto events = eventQueue->Drain();
        bool shouldFallbackToPlain = false;

        for (auto& event : events)
        {
            switch (event.type)
            {
            case EventQueue::Type::Open:
                state = State::SocketConnected;
                reconnectInterval = std::chrono::milliseconds{ 1500 };
                pendingDataPackageRequests = 0;
                break;

            case EventQueue::Type::TlsHandshakeFailed:
                if (state == State::SocketConnecting)
                {
                    shouldFallbackToPlain = true;
                }
                else if (onSocketError)
                {
                    // TLS error on an established connection: report it like any
                    // other socket error instead of downgrading to plaintext.
                    onSocketError(event.data);
                }
                break;

            case EventQueue::Type::Close:
                if (state > State::SocketConnecting)
                {
                    state = State::Disconnected;
                    if (onSocketDisconnected) onSocketDisconnected();
                }
                else if (!shouldFallbackToPlain)
                {
                    // Failed connection attempt: alternate between TLS and plain
                    // until one succeeds. Once connected, the working scheme is
                    // kept for later reconnects.
                    currentAttemptTls = !currentAttemptTls;
                }
                if (!shouldFallbackToPlain)
                {
                    state = State::Disconnected;
                }
                break;

            case EventQueue::Type::Message:
                ProcessMessage(event.data);
                break;

            case EventQueue::Type::Error:
                if (onSocketError) onSocketError(event.data);
                break;
            }
        }

        if (shouldFallbackToPlain)
        {
            ConnectSocket(false);
            return;
        }

        if (state < State::SocketConnected)
        {
            auto now = std::chrono::steady_clock::now();
            if (reconnectNow || (now - lastConnectAttempt >= reconnectInterval))
            {
                ConnectSocket(currentAttemptTls);
            }
        }
    }

    // ---------------------------------------------------------------------------
    // Reset
    // ---------------------------------------------------------------------------

    void Client::Reset()
    {
        // Drain pending events so stale callbacks can't fire after reset.
        eventQueue->Drain();

        if (ws)
        {
            ws->Stop();
        }
        ws.reset();
        state = State::Disconnected;
        currentAttemptTls = preferTls;
        reconnectNow = true;
        reconnectInterval = std::chrono::milliseconds{ 1500 };
        lastConnectAttempt = std::chrono::steady_clock::time_point{};

        checkQueue.clear();
        clientStatus = ClientStatus::Unknown;
        slotName.clear();
        team = -1;
        slotnr = -1;
        players.clear();
        slotInfo.clear();
        serverChecksums.clear();
        gameItemMap.clear();
        gameLocationMap.clear();
        pendingDataPackageRequests = 0;
        serverConnectTime = 0.0;
        localConnectTime = {};
    }

    // ---------------------------------------------------------------------------
    // Protocol Commands
    // ---------------------------------------------------------------------------

    bool Client::ConnectSlot(const std::string& name, const std::string& password,
        int itemsHandling, const std::list<std::string>& tags)
    {
        if (state < State::SocketConnected)
        {
            return false;
        }

        slotName = name;

        json packet = json::array({ json({
            {"cmd", "Connect"},
            {"game", game},
            {"uuid", uuid},
            {"name", name},
            {"password", password},
            {"version", {{"major", 0}, {"minor", 5}, {"build", 0}, {"class", "Version"}}},
            {"items_handling", itemsHandling},
            {"tags", tags}
        }) });

        Send(packet);
        return true;
    }

    bool Client::ConnectUpdate(int itemsHandling, const std::list<std::string>& tags)
    {
        if (state < State::SocketConnected)
        {
            return false;
        }

        json packet = json::array({ json({
            {"cmd", "ConnectUpdate"},
            {"items_handling", itemsHandling},
            {"tags", tags}
        }) });

        Send(packet);
        return true;
    }

    bool Client::LocationChecks(const std::list<int64_t>& locations)
    {
        if (state == State::SlotConnected)
        {
            json packet = json::array({ json({
                {"cmd", "LocationChecks"},
                {"locations", locations}
            }) });

            Send(packet);
        }
        else
        {
            checkQueue.insert(locations.begin(), locations.end());
        }

        return true;
    }

    bool Client::StatusUpdate(ClientStatus status)
    {
        if (state == State::SlotConnected)
        {
            json packet = json::array({ json({
                {"cmd", "StatusUpdate"},
                {"status", static_cast<int>(status)}
            }) });

            Send(packet);
            return true;
        }

        clientStatus = status;
        return false;
    }

    bool Client::GetDataPackage(const std::list<std::string>& games)
    {
        if (state < State::RoomInfo)
        {
            return false;
        }

        std::list<std::string> outdated;
        for (const auto& game : games)
        {
            if (!TryUseCachedDataPackage(game))
            {
                outdated.push_back(game);
            }
        }

        if (outdated.empty())
        {
            // Everything is already up to date (possibly restored from cache).
            if (onDataPackageChanged) onDataPackageChanged(dataPackage);
            return true;
        }

        json packet = json::array({ json({
            {"cmd", "GetDataPackage"},
            {"games", outdated}
        }) });

        pendingDataPackageRequests++;
        Send(packet);
        return true;
    }

    bool Client::Bounce(const json& data, const std::list<std::string>& games,
        const std::list<int>& slots, const std::list<std::string>& tags)
    {
        if (state < State::RoomInfo)
        {
            return false;
        }

        json cmd = {
            {"cmd", "Bounce"},
            {"data", data}
        };

        if (!games.empty()) cmd["games"] = games;
        if (!slots.empty()) cmd["slots"] = slots;
        if (!tags.empty()) cmd["tags"] = tags;

        Send(json::array({ cmd }));
        return true;
    }

    // ---------------------------------------------------------------------------
    // Queries
    // ---------------------------------------------------------------------------

    Client::State Client::GetState() const
    {
        return state;
    }

    int Client::GetPlayerNumber() const
    {
        return slotnr;
    }

    std::string Client::GetPlayerAlias(int slot) const
    {
        if (slot == 0)
        {
            return "Server";
        }

        for (const auto& p : players)
        {
            if (p.team == team && p.slot == slot)
            {
                return p.alias;
            }
        }

        return "Unknown";
    }

    std::string Client::GetPlayerGame(int player) const
    {
        if (player == 0)
        {
            return "Archipelago";
        }

        auto it = slotInfo.find(player);
        if (it != slotInfo.end())
        {
            return it->second.game;
        }

        return "";
    }

    std::string Client::GetItemName(int64_t code, const std::string& game) const
    {
        static const std::string archipelago = "Archipelago";

        for (const auto& g : { game, archipelago })
        {
            auto gIt = gameItemMap.find(g);
            if (gIt != gameItemMap.end())
            {
                auto it = gIt->second.find(code);
                if (it != gIt->second.end())
                {
                    return it->second;
                }
            }
        }

        return "Unknown";
    }

    std::string Client::GetLocationName(int64_t code, const std::string& game) const
    {
        static const std::string archipelago = "Archipelago";

        for (const auto& g : { game, archipelago })
        {
            auto gIt = gameLocationMap.find(g);
            if (gIt != gameLocationMap.end())
            {
                auto it = gIt->second.find(code);
                if (it != gIt->second.end())
                {
                    return it->second;
                }
            }
        }

        return "Unknown";
    }

    double Client::GetServerTime() const
    {
        if (localConnectTime == std::chrono::steady_clock::time_point{})
        {
            // No RoomInfo received yet; there is no server time to extrapolate.
            return 0.0;
        }

        auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - localConnectTime);
        return serverConnectTime + elapsed.count();
    }

    std::list<std::string> Client::GetAllGames() const
    {
        std::set<std::string> seen;
        std::list<std::string> result;

        for (const auto& [slot, info] : slotInfo)
        {
            if (info.game.empty() || info.game == "Archipelago")
            {
                continue;
            }

            if (seen.insert(info.game).second)
            {
                result.push_back(info.game);
            }
        }

        return result;
    }

    bool Client::SlotConcernsSelf(int slot) const
    {
        if (slot == slotnr)
        {
            return true;
        }

        auto it = slotInfo.find(slot);
        if (it != slotInfo.end())
        {
            const auto& members = it->second.members;
            return std::find(members.begin(), members.end(), slotnr) != members.end();
        }

        return false;
    }

    std::string Client::RenderJson(const std::list<TextNode>& msg, RenderFormat fmt) const
    {
        std::string out;
        bool colorIsSet = false;

        for (const auto& node : msg)
        {
            std::string color;
            std::string text;

            if (fmt != RenderFormat::Text)
            {
                color = node.color;
            }

            if (node.type == "player_id")
            {
                int id = std::stoi(node.text);
                if (color.empty() && SlotConcernsSelf(id)) color = "magenta";
                else if (color.empty()) color = "yellow";
                text = GetPlayerAlias(id);
            }
            else if (node.type == "item_id")
            {
                int64_t id = std::stoll(node.text);
                if (color.empty())
                {
                    if (node.flags & FlagAdvancement) color = "plum";
                    else if (node.flags & FlagNeverExclude) color = "slateblue";
                    else if (node.flags & FlagTrap) color = "salmon";
                    else color = "cyan";
                }
                text = GetItemName(id, GetPlayerGame(node.player));
            }
            else if (node.type == "location_id")
            {
                int64_t id = std::stoll(node.text);
                if (color.empty()) color = "blue";
                text = GetLocationName(id, GetPlayerGame(node.player));
            }
            else if (node.type == "hint_status")
            {
                text = node.text;
                if (node.hintStatus == HintFound) color = "green";
                else if (node.hintStatus == HintUnspecified) color = "grey";
                else if (node.hintStatus == HintNoPriority) color = "slateblue";
                else if (node.hintStatus == HintAvoid) color = "salmon";
                else if (node.hintStatus == HintPriority) color = "plum";
                else color = "red";
            }
            else
            {
                text = node.text;
            }

            if (fmt == RenderFormat::Ansi)
            {
                if (color.empty() && colorIsSet)
                {
                    out += Color2Ansi("");
                    colorIsSet = false;
                }
                else if (!color.empty())
                {
                    out += Color2Ansi(color);
                    colorIsSet = true;
                }
                Deansify(text);
                out += text;
            }
            else
            {
                out += text;
            }
        }

        if (fmt == RenderFormat::Ansi && colorIsSet)
        {
            out += Color2Ansi("");
        }

        return out;
    }

    // ---------------------------------------------------------------------------
    // Callback Setters
    // ---------------------------------------------------------------------------

    void Client::SetSlotConnectedHandler(std::function<void(const json&)> handler) { onSlotConnected = std::move(handler); }
    void Client::SetSlotRefusedHandler(std::function<void(const std::list<std::string>&)> handler) { onSlotRefused = std::move(handler); }
    void Client::SetSocketErrorHandler(std::function<void(const std::string&)> handler) { onSocketError = std::move(handler); }
    void Client::SetSocketDisconnectedHandler(std::function<void()> handler) { onSocketDisconnected = std::move(handler); }
    void Client::SetRoomInfoHandler(std::function<void()> handler) { onRoomInfo = std::move(handler); }
    void Client::SetDataPackageChangedHandler(std::function<void(const json&)> handler) { onDataPackageChanged = std::move(handler); }
    void Client::SetItemsReceivedHandler(std::function<void(const std::list<NetworkItem>&)> handler) { onItemsReceived = std::move(handler); }
    void Client::SetPrintJsonHandler(std::function<void(const std::list<TextNode>&)> handler) { onPrintJson = std::move(handler); }
    void Client::SetBouncedHandler(std::function<void(const json&)> handler) { onBounced = std::move(handler); }
    void Client::SetMessageErrorHandler(std::function<void(const std::string&)> handler) { onMessageError = std::move(handler); }

    // ---------------------------------------------------------------------------
    // Private - socket management
    // ---------------------------------------------------------------------------

    void Client::ConnectSocket(bool useTls)
    {
        reconnectNow = false;
        if (ws)
        {
            ws->Stop();
        }
        ws.reset();

        // Replace the event queue so any late events from the previous socket
        // are pushed into the old (no longer drained) queue and silently discarded.
        eventQueue = std::make_shared<EventQueue>();

        state = State::SocketConnecting;
        currentAttemptTls = useTls;
        ws = wsService.CreateClient(useTls);

        // Capture only the shared EventQueue - never capture `this`.
        auto eq(eventQueue);

        ws->SetOpenHandler([eq]() { eq->Push(EventQueue::Type::Open); });
        ws->SetCloseHandler([eq]() { eq->Push(EventQueue::Type::Close); });
        ws->SetMessageHandler([eq](std::string msg) { eq->Push(EventQueue::Type::Message, std::move(msg)); });
        ws->SetErrorHandler([eq](boost::beast::error_code ec)
        {
            if (ec.category() == boost::asio::error::get_ssl_category())
            {
                eq->Push(EventQueue::Type::TlsHandshakeFailed, ec.message());
            }
            else
            {
                eq->Push(EventQueue::Type::Error, ec.message());
            }
        });

        ws->Connect(host, port);

        lastConnectAttempt = std::chrono::steady_clock::now();
        reconnectInterval = std::min(reconnectInterval * 2, std::chrono::milliseconds{ 15000 });
    }

    void Client::Send(const json& packet)
    {
        if (ws && ws->GetState() == WebSocketClient::State::Connected)
        {
            ws->Send(packet.dump());
        }
    }

    // ---------------------------------------------------------------------------
    // Private - message processing
    // ---------------------------------------------------------------------------

    void Client::ProcessMessage(const std::string& message)
    {
        json packet = json::parse(message, nullptr, false);
        if (packet.is_discarded() || !packet.is_array())
        {
            if (onMessageError) onMessageError("received malformed packet");
            return;
        }

        // Isolate each command so one malformed command cannot abort the rest
        // of the packet. Required fields are read with at(), which throws on
        // missing keys instead of the undefined behavior of const operator[].
        for (const auto& command : packet)
        {
            try
            {
                ProcessCommand(command);
            }
            catch (const std::exception& ex)
            {
                if (onMessageError) onMessageError(ex.what());
            }
        }
    }

    void Client::ProcessCommand(const json& command)
    {
        std::string cmd = command.value("cmd", "");

        if (cmd == "RoomInfo")
        {
            localConnectTime = std::chrono::steady_clock::now();
            serverConnectTime = command.at("time").get<double>();

            serverChecksums.clear();
            if (command.contains("datapackage_checksums") && command["datapackage_checksums"].is_object())
            {
                for (const auto& [gameName, checksum] : command["datapackage_checksums"].items())
                {
                    if (checksum.is_string())
                    {
                        serverChecksums[gameName] = checksum.get<std::string>();
                    }
                }
            }

            if (state < State::RoomInfo)
            {
                state = State::RoomInfo;
            }

            if (onRoomInfo) onRoomInfo();
        }
        else if (cmd == "ConnectionRefused")
        {
            if (onSlotRefused)
            {
                std::list<std::string> errors;
                for (const auto& error : command.at("errors"))
                {
                    errors.push_back(error.get<std::string>());
                }

                onSlotRefused(errors);
            }
        }
        else if (cmd == "Connected")
        {
            // Read all required fields before mutating any state.
            int newTeam = command.at("team").get<int>();
            int newSlot = command.at("slot").get<int>();
            const json& playerList = command.at("players");
            const json& slotData = command.at("slot_data");

            state = State::SlotConnected;
            team = newTeam;
            slotnr = newSlot;

            players.clear();
            for (const auto& p : playerList)
            {
                players.push_back({
                    p.at("team").get<int>(),
                    p.at("slot").get<int>(),
                    p.at("alias").get<std::string>(),
                    p.at("name").get<std::string>()
                });
            }

            slotInfo.clear();
            if (command.contains("slot_info") && command["slot_info"].is_object())
            {
                for (const auto& [key, value] : command["slot_info"].items())
                {
                    NetworkSlot ns;
                    value.at("name").get_to(ns.name);
                    value.at("game").get_to(ns.game);
                    value.at("type").get_to(ns.type);
                    value.at("group_members").get_to(ns.members);
                    slotInfo[std::stoi(key)] = std::move(ns);
                }
            }

            // Flush queued location checks
            if (!checkQueue.empty())
            {
                std::list<int64_t> queued(checkQueue.begin(), checkQueue.end());
                checkQueue.clear();
                LocationChecks(queued);
            }

            // Flush queued status
            if (clientStatus != ClientStatus::Unknown)
            {
                StatusUpdate(clientStatus);
            }

            if (onSlotConnected) onSlotConnected(slotData);
        }
        else if (cmd == "ReceivedItems")
        {
            std::list<NetworkItem> items;
            int index = command.at("index").get<int>();

            for (const auto& j : command.at("items"))
            {
                NetworkItem item;
                item.item = j.at("item").get<int64_t>();
                item.location = j.at("location").get<int64_t>();
                item.player = j.at("player").get<int>();
                item.flags = j.value("flags", 0U);
                item.index = index++;
                items.push_back(item);
            }

            if (onItemsReceived) onItemsReceived(items);
        }
        else if (cmd == "DataPackage")
        {
            for (const auto& [gameName, gameData] : command.at("data").at("games").items())
            {
                ApplyGameData(gameName, gameData);
                SaveDataPackageCache(gameData.value("checksum", ""), gameData);
            }

            if (pendingDataPackageRequests > 0)
            {
                pendingDataPackageRequests--;
                if (pendingDataPackageRequests == 0)
                {
                    if (onDataPackageChanged) onDataPackageChanged(dataPackage);
                }
            }
        }
        else if (cmd == "PrintJSON")
        {
            if (onPrintJson)
            {
                std::list<TextNode> data;
                for (const auto& part : command.at("data"))
                {
                    data.push_back(TextNode::FromJson(part));
                }

                onPrintJson(data);
            }
        }
        else if (cmd == "Bounced")
        {
            if (onBounced) onBounced(command);
        }
        else if (cmd == "RoomUpdate")
        {
            if (command.contains("players") && command["players"].is_array())
            {
                players.clear();
                for (const auto& p : command["players"])
                {
                    players.push_back({
                        p.at("team").get<int>(),
                        p.at("slot").get<int>(),
                        p.at("alias").get<std::string>(),
                        p.at("name").get<std::string>()
                    });
                }
            }
        }
    }

    // ---------------------------------------------------------------------------
    // Private - data package
    // ---------------------------------------------------------------------------

    void Client::ApplyGameData(const std::string& gameName, const json& gameData)
    {
        dataPackage["games"][gameName] = gameData;

        if (gameData.contains("item_name_to_id"))
        {
            auto& gi = gameItemMap[gameName];
            for (const auto& [name, id] : gameData["item_name_to_id"].items())
            {
                gi[id.get<int64_t>()] = name;
            }
        }

        if (gameData.contains("location_name_to_id"))
        {
            auto& gl = gameLocationMap[gameName];
            for (const auto& [name, id] : gameData["location_name_to_id"].items())
            {
                gl[id.get<int64_t>()] = name;
            }
        }
    }

    bool Client::TryUseCachedDataPackage(const std::string& game)
    {
        auto checksumIt = serverChecksums.find(game);
        if (checksumIt == serverChecksums.end())
        {
            // The server did not announce a checksum for this game; always fetch.
            return false;
        }

        const std::string& checksum = checksumIt->second;

        auto& games = dataPackage["games"];
        if (auto it = games.find(game); it != games.end() && it->value("checksum", "") == checksum)
        {
            return true;
        }

        json cached = LoadDataPackageCache(checksum);
        if (cached.is_object() && cached.value("checksum", "") == checksum)
        {
            ApplyGameData(game, cached);
            return true;
        }

        return false;
    }

    // ---------------------------------------------------------------------------
    // Private - ANSI helpers
    // ---------------------------------------------------------------------------

    std::string Client::Color2Ansi(const std::string& color)
    {
        if (color == "red") return "\x1b[31m";
        if (color == "green") return "\x1b[32m";
        if (color == "yellow") return "\x1b[33m";
        if (color == "blue") return "\x1b[34m";
        if (color == "magenta") return "\x1b[35m";
        if (color == "cyan") return "\x1b[36m";
        if (color == "plum") return "\x1b[38:5:219m";
        if (color == "slateblue") return "\x1b[38:5:62m";
        if (color == "salmon") return "\x1b[38:5:210m";
        if (color == "gray") return "\x1b[90m";
        if (color == "grey") return "\x1b[90m";
        return "\x1b[0m";
    }

    void Client::Deansify(std::string& text)
    {
        std::replace(text.begin(), text.end(), '\x1b', ' ');
    }
}
