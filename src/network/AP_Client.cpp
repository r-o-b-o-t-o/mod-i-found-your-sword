#include "network/AP_Client.h"
#include "network/AP_WebSocketClient.h"
#include "network/AP_WebSocketService.h"

#include <algorithm>
#include <iostream>

namespace ModArchipelaWoW::Network
{
    // ---------------------------------------------------------------------------
    // EventQueue - thread-safe bridge between the io_context and game threads
    // ---------------------------------------------------------------------------

    struct Client::EventQueue
    {
        enum class Type { Open, Close, Message, Error };

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
            events.push_back({type, std::move(data)});
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
    // Version
    // ---------------------------------------------------------------------------

    Client::Version Client::Version::FromJson(const json& j)
    {
        if (j.is_null())
            return {0, 0, 0};

        return {
            j.value("major", 0),
            j.value("minor", 0),
            j.value("build", 0)
        };
    }

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
        , dataPackage({{"version", -1}, {"games", json::object()}})
    {
    }

    Client::~Client()
    {
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
                reconnectInterval = std::chrono::milliseconds{1500};
                pendingDataPackageRequests = 0;
                serverVersion = {};
                break;

            case EventQueue::Type::Close:
                if (state == State::SocketConnecting && currentAttemptTls)
                {
                    // TLS handshake failed - fall back to plain
                    shouldFallbackToPlain = true;
                }
                else if (state > State::SocketConnecting)
                {
                    state = State::Disconnected;
                    if (onSocketDisconnected) onSocketDisconnected();
                }
                if (!shouldFallbackToPlain)
                {
                    state = State::Disconnected;
                    seed.clear();
                }
                break;

            case EventQueue::Type::Message:
                ProcessMessage(event.data);
                break;

            case EventQueue::Type::Error:
                // Suppress error callback during TLS fallback attempt
                if (!(state == State::SocketConnecting && currentAttemptTls) && onSocketError)
                {
                    onSocketError(event.data);
                }
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
                ConnectSocket();
            }
        }
    }

    // ---------------------------------------------------------------------------
    // Reset
    // ---------------------------------------------------------------------------

    void Client::Reset()
    {
        checkQueue.clear();
        clientStatus = ClientStatus::Unknown;
        seed.clear();
        slotName.clear();
        team = -1;
        slotnr = -1;
        players.clear();
        ws.reset();
        state = State::Disconnected;
    }

    // ---------------------------------------------------------------------------
    // Protocol Commands
    // ---------------------------------------------------------------------------

    bool Client::ConnectSlot(const std::string& name, const std::string& password,
                            int itemsHandling, const std::list<std::string>& tags)
    {
        if (state < State::SocketConnected)
            return false;

        slotName = name;

        json packet = json::array({json({
            {"cmd", "Connect"},
            {"game", game},
            {"uuid", uuid},
            {"name", name},
            {"password", password},
            {"version", {{"major", 0}, {"minor", 5}, {"build", 0}, {"class", "Version"}}},
            {"items_handling", itemsHandling},
            {"tags", tags}
        })});

        Send(packet);
        return true;
    }

    bool Client::ConnectUpdate(int itemsHandling, const std::list<std::string>& tags)
    {
        if (state < State::SocketConnected)
            return false;

        json packet = json::array({json({
            {"cmd", "ConnectUpdate"},
            {"items_handling", itemsHandling},
            {"tags", tags}
        })});

        Send(packet);
        return true;
    }

    bool Client::LocationChecks(const std::list<int64_t>& locations)
    {
        if (state == State::SlotConnected)
        {
            json packet = json::array({json({
                {"cmd", "LocationChecks"},
                {"locations", locations}
            })});

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
            json packet = json::array({json({
                {"cmd", "StatusUpdate"},
                {"status", static_cast<int>(status)}
            })});

            Send(packet);
            return true;
        }

        clientStatus = status;
        return false;
    }

    bool Client::GetDataPackage(const std::list<std::string>& games)
    {
        if (state < State::RoomInfo)
            return false;

        json packet = json::array({json({
            {"cmd", "GetDataPackage"},
            {"games", games}
        })});

        pendingDataPackageRequests++;
        Send(packet);
        return true;
    }

    bool Client::Bounce(const json& data, const std::list<std::string>& games,
                        const std::list<int>& slots, const std::list<std::string>& tags)
    {
        if (state < State::RoomInfo)
            return false;

        json cmd = {
            {"cmd", "Bounce"},
            {"data", data}
        };

        if (!games.empty()) cmd["games"] = games;
        if (!slots.empty()) cmd["slots"] = slots;
        if (!tags.empty()) cmd["tags"] = tags;

        Send(json::array({cmd}));
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
            return "Server";

        for (const auto& p : players)
        {
            if (p.team == team && p.slot == slot)
                return p.alias;
        }

        return "Unknown";
    }

    std::string Client::GetPlayerGame(int player) const
    {
        if (player == 0)
            return "Archipelago";

        auto it = slotInfo.find(player);
        if (it != slotInfo.end())
            return it->second.game;

        return "";
    }

    std::string Client::GetItemName(int64_t code, const std::string& game) const
    {
        static const std::string archipelago = "Archipelago";

        for (const auto& g : {game, archipelago})
        {
            auto gIt = gameItemMap.find(g);
            if (gIt != gameItemMap.end())
            {
                auto it = gIt->second.find(code);
                if (it != gIt->second.end())
                    return it->second;
            }
        }

        return "Unknown";
    }

    std::string Client::GetLocationName(int64_t code, const std::string& game) const
    {
        static const std::string archipelago = "Archipelago";

        for (const auto& g : {game, archipelago})
        {
            auto gIt = gameLocationMap.find(g);
            if (gIt != gameLocationMap.end())
            {
                auto it = gIt->second.find(code);
                if (it != gIt->second.end())
                    return it->second;
            }
        }

        return "Unknown";
    }

    double Client::GetServerTime() const
    {
        auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - localConnectTime);
        return serverConnectTime + elapsed.count();
    }

    bool Client::SlotConcernsSelf(int slot) const
    {
        if (slot == slotnr)
            return true;

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
                color = node.color;

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
            out += Color2Ansi("");

        return out;
    }

    // ---------------------------------------------------------------------------
    // Callback Setters
    // ---------------------------------------------------------------------------

    void Client::SetSlotConnectedHandler(std::function<void(const json&)> handler) { onSlotConnected = std::move(handler); }
    void Client::SetSlotRefusedHandler(std::function<void(const std::list<std::string>&)> handler) { onSlotRefused = std::move(handler); }
    void Client::SetSocketErrorHandler(std::function<void(const std::string&)> handler) { onSocketError = std::move(handler); }
    void Client::SetSocketDisconnectedHandler(std::function<void()> handler) { onSocketDisconnected = std::move(handler); }
    void Client::SetSlotDisconnectedHandler(std::function<void()> handler) { onSlotDisconnected = std::move(handler); }
    void Client::SetRoomInfoHandler(std::function<void()> handler) { onRoomInfo = std::move(handler); }
    void Client::SetDataPackageChangedHandler(std::function<void(const json&)> handler) { onDataPackageChanged = std::move(handler); }
    void Client::SetItemsReceivedHandler(std::function<void(const std::list<NetworkItem>&)> handler) { onItemsReceived = std::move(handler); }
    void Client::SetPrintJsonHandler(std::function<void(const std::list<TextNode>&)> handler) { onPrintJson = std::move(handler); }
    void Client::SetBouncedHandler(std::function<void(const json&)> handler) { onBounced = std::move(handler); }

    // ---------------------------------------------------------------------------
    // Private - socket management
    // ---------------------------------------------------------------------------

    void Client::ConnectSocket(bool useTls)
    {
        reconnectNow = false;
        ws.reset();

        state = State::SocketConnecting;
        currentAttemptTls = useTls;
        ws = wsService.CreateClient(useTls);

        // Capture only the shared EventQueue - never capture `this`.
        auto eq = eventQueue;

        ws->SetOpenHandler([eq]() { eq->Push(EventQueue::Type::Open); });
        ws->SetCloseHandler([eq]() { eq->Push(EventQueue::Type::Close); });
        ws->SetMessageHandler([eq](std::string msg) { eq->Push(EventQueue::Type::Message, std::move(msg)); });
        ws->SetErrorHandler([eq](boost::beast::error_code ec) { eq->Push(EventQueue::Type::Error, ec.message()); });

        ws->Connect(host, port);

        lastConnectAttempt = std::chrono::steady_clock::now();
        reconnectInterval = std::min(reconnectInterval * 2, std::chrono::milliseconds{15000});
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
        try
        {
            json packet = json::parse(message);
            if (!packet.is_array())
                return;

            for (auto& command : packet)
            {
                ProcessCommand(command);
            }
        }
        catch (const std::exception& ex)
        {
            std::cerr << "APClient: message processing error: " << ex.what() << std::endl;
        }
    }

    void Client::ProcessCommand(const json& command)
    {
        std::string cmd = command.value("cmd", "");

        if (cmd == "RoomInfo")
        {
            localConnectTime = std::chrono::steady_clock::now();
            serverConnectTime = command["time"].get<double>();
            serverVersion = Version::FromJson(command["version"]);
            seed = command.value("seed_name", "");

            if (state < State::RoomInfo)
                state = State::RoomInfo;

            if (onRoomInfo) onRoomInfo();
        }
        else if (cmd == "ConnectionRefused")
        {
            if (onSlotRefused)
            {
                std::list<std::string> errors;
                for (const auto& error : command["errors"])
                    errors.push_back(error.get<std::string>());

                onSlotRefused(errors);
            }
        }
        else if (cmd == "Connected")
        {
            state = State::SlotConnected;
            team = command["team"].get<int>();
            slotnr = command["slot"].get<int>();

            players.clear();
            for (const auto& p : command["players"])
            {
                players.push_back({
                    p["team"].get<int>(),
                    p["slot"].get<int>(),
                    p["alias"].get<std::string>(),
                    p["name"].get<std::string>()
                });
            }

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

            if (onSlotConnected)
                onSlotConnected(command["slot_data"]);
        }
        else if (cmd == "ReceivedItems")
        {
            std::list<NetworkItem> items;
            int index = command["index"].get<int>();

            for (const auto& j : command["items"])
            {
                NetworkItem item;
                item.item = j["item"].get<int64_t>();
                item.location = j["location"].get<int64_t>();
                item.player = j["player"].get<int>();
                item.flags = j.value("flags", 0U);
                item.index = index++;
                items.push_back(item);
            }

            if (onItemsReceived) onItemsReceived(items);
        }
        else if (cmd == "DataPackage")
        {
            auto data = dataPackage;
            if (!data["games"].is_object())
                data["games"] = json::object();

            for (const auto& [gameName, gameData] : command["data"]["games"].items())
            {
                data["games"][gameName] = gameData;
            }

            SetDataPackageData(data);

            if (pendingDataPackageRequests > 0)
            {
                pendingDataPackageRequests--;
                if (pendingDataPackageRequests == 0)
                {
                    dataPackageValid = true;
                    if (onDataPackageChanged) onDataPackageChanged(dataPackage);
                }
            }
        }
        else if (cmd == "PrintJSON")
        {
            if (onPrintJson)
            {
                std::list<TextNode> data;
                for (const auto& part : command["data"])
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
                        p["team"].get<int>(),
                        p["slot"].get<int>(),
                        p["alias"].get<std::string>(),
                        p["name"].get<std::string>()
                    });
                }
            }
        }
    }

    // ---------------------------------------------------------------------------
    // Private - data package
    // ---------------------------------------------------------------------------

    void Client::SetDataPackageData(const json& data)
    {
        dataPackage = data;

        for (const auto& [gameName, gameData] : dataPackage["games"].items())
        {
            if (gameData.contains("item_name_to_id"))
            {
                auto& gi = gameItemMap[gameName];
                for (const auto& [name, id] : gameData["item_name_to_id"].items())
                {
                    auto idVal = id.get<int64_t>();
                    itemNameMap[idVal] = name;
                    gi[idVal] = name;
                }
            }

            if (gameData.contains("location_name_to_id"))
            {
                auto& gl = gameLocationMap[gameName];
                for (const auto& [name, id] : gameData["location_name_to_id"].items())
                {
                    auto idVal = id.get<int64_t>();
                    locationNameMap[idVal] = name;
                    gl[idVal] = name;
                }
            }
        }
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
