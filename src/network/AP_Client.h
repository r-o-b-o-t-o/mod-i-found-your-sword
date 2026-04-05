#ifndef _MOD_ARCHIPELAWOW_NETWORK_CLIENT_H_
#define _MOD_ARCHIPELAWOW_NETWORK_CLIENT_H_

#include <chrono>
#include <cstdint>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace ModArchipelaWoW::Network
{
    class WebSocketClient;
    class WebSocketService;

    /// Archipelago Protocol client.
    ///
    /// All state lives on the game thread. The underlying WebSocket pushes
    /// raw events into a thread-safe queue that Poll() drains and processes.
    class Client
    {
    public:
        using json = nlohmann::json;

        enum class State
        {
            Disconnected,
            SocketConnecting,
            SocketConnected,
            RoomInfo,
            SlotConnected
        };

        enum class ClientStatus : int
        {
            Unknown = 0,
            Ready = 10,
            Playing = 20,
            Goal = 30
        };

        enum class RenderFormat
        {
            Text,
            Ansi
        };

        enum ItemFlags
        {
            FlagNone = 0,
            FlagAdvancement = 1,
            FlagNeverExclude = 2,
            FlagTrap = 4
        };

        enum HintStatus
        {
            HintUnspecified = 0,
            HintNoPriority = 10,
            HintAvoid = 20,
            HintPriority = 30,
            HintFound = 40
        };

        struct Version
        {
            int ma = 0;
            int mi = 0;
            int build = 0;

            static Version FromJson(const json& j);
        };

        struct NetworkItem
        {
            int64_t item = 0;
            int64_t location = 0;
            int player = 0;
            unsigned flags = 0;
            int index = -1;
        };

        struct NetworkPlayer
        {
            int team = 0;
            int slot = 0;
            std::string alias;
            std::string name;
        };

        struct NetworkSlot
        {
            std::string name;
            std::string game;
            int type = 0;
            std::list<int> members;
        };

        struct TextNode
        {
            std::string type;
            std::string color;
            std::string text;
            int player = 0;
            unsigned flags = FlagNone;
            unsigned hintStatus = HintUnspecified;

            static TextNode FromJson(const json& j);
        };

        Client(WebSocketService& wsService, const std::string& uuid, const std::string& game,
               const std::string& host, const std::string& port);
        ~Client();

        Client(const Client&) = delete;
        Client& operator=(const Client&) = delete;

        /// Process network events and drive reconnection. Call once per frame.
        void Poll();

        /// Clear all state and reconnect on next Poll().
        void Reset();

        /// Send a Connect command to join the named slot.
        bool ConnectSlot(const std::string& name, const std::string& password,
                         int itemsHandling, const std::list<std::string>& tags = {});

        /// Send a ConnectUpdate command to change items handling or tags.
        bool ConnectUpdate(int itemsHandling, const std::list<std::string>& tags);

        /// Send checked locations to the server.
        bool LocationChecks(const std::list<int64_t>& locations);

        /// Send a status update (e.g. Goal reached).
        bool StatusUpdate(ClientStatus status);

        /// Request the data package for the listed games.
        bool GetDataPackage(const std::list<std::string>& games);

        /// Send a Bounce packet.
        bool Bounce(const json& data, const std::list<std::string>& games = {},
                    const std::list<int>& slots = {}, const std::list<std::string>& tags = {});

        State GetState() const;
        int GetPlayerNumber() const;
        std::string GetPlayerAlias(int slot) const;
        std::string GetPlayerGame(int player) const;
        std::string GetItemName(int64_t code, const std::string& game) const;
        std::string GetLocationName(int64_t code, const std::string& game) const;
        double GetServerTime() const;
        std::string RenderJson(const std::list<TextNode>& msg, RenderFormat fmt = RenderFormat::Text) const;
        bool SlotConcernsSelf(int slot) const;

        void SetSlotConnectedHandler(std::function<void(const json&)> handler);
        void SetSlotRefusedHandler(std::function<void(const std::list<std::string>&)> handler);
        void SetSocketErrorHandler(std::function<void(const std::string&)> handler);
        void SetSocketDisconnectedHandler(std::function<void()> handler);
        void SetSlotDisconnectedHandler(std::function<void()> handler);
        void SetRoomInfoHandler(std::function<void()> handler);
        void SetDataPackageChangedHandler(std::function<void(const json&)> handler);
        void SetItemsReceivedHandler(std::function<void(const std::list<NetworkItem>&)> handler);
        void SetPrintJsonHandler(std::function<void(const std::list<TextNode>&)> handler);
        void SetBouncedHandler(std::function<void(const json&)> handler);

    private:
        struct EventQueue;

        void ConnectSocket(bool useTls = true);
        void ProcessMessage(const std::string& message);
        void ProcessCommand(const json& command);
        void Send(const json& packet);
        void SetDataPackageData(const json& data);

        static std::string Color2Ansi(const std::string& color);
        static void Deansify(std::string& text);

        WebSocketService& wsService;
        std::string uuid;
        std::string game;
        std::string host;
        std::string port;
        bool currentAttemptTls = true;

        std::shared_ptr<WebSocketClient> ws;
        std::shared_ptr<EventQueue> eventQueue;

        State state = State::Disconnected;
        ClientStatus clientStatus = ClientStatus::Unknown;
        std::string seed;
        std::string slotName;
        int team = -1;
        int slotnr = -1;

        std::chrono::steady_clock::time_point lastConnectAttempt;
        std::chrono::milliseconds reconnectInterval{1500};
        bool reconnectNow = true;

        std::set<int64_t> checkQueue;

        std::list<NetworkPlayer> players;
        std::map<int, NetworkSlot> slotInfo;
        json dataPackage;
        std::map<int64_t, std::string> itemNameMap;
        std::map<int64_t, std::string> locationNameMap;
        std::map<std::string, std::map<int64_t, std::string>> gameItemMap;
        std::map<std::string, std::map<int64_t, std::string>> gameLocationMap;
        bool dataPackageValid = false;
        size_t pendingDataPackageRequests = 0;

        double serverConnectTime = 0.0;
        std::chrono::steady_clock::time_point localConnectTime;
        Version serverVersion{};

        std::function<void(const json&)> onSlotConnected;
        std::function<void(const std::list<std::string>&)> onSlotRefused;
        std::function<void(const std::string&)> onSocketError;
        std::function<void()> onSocketDisconnected;
        std::function<void()> onSlotDisconnected;
        std::function<void()> onRoomInfo;
        std::function<void(const json&)> onDataPackageChanged;
        std::function<void(const std::list<NetworkItem>&)> onItemsReceived;
        std::function<void(const std::list<TextNode>&)> onPrintJson;
        std::function<void(const json&)> onBounced;
    };
}

#endif
