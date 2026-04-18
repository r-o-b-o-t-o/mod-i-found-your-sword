#ifndef _MOD_ARCHIPELAWOW_AP_CHARACTER_H_
#define _MOD_ARCHIPELAWOW_AP_CHARACTER_H_

#include "AP_PlayerPosition.h"
#include "AP_Stone.h"
#include "DBCStructure.h"
#include "Define.h"
#include "Item.h"
#include "items/AP_ItemsContainer.h"
#include "items/AP_Zones.h"
#include "locations/AP_LocationsContainer.h"
#include "network/AP_Client.h"
#include "nlohmann/json.hpp"
#include "Player.h"
#include "QuestDef.h"
#include "Unit.h"

#include <chrono>
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <unordered_set>

namespace ModArchipelaWoW
{
    class AP_Character
    {
    public:
        AP_Character(Player* player, std::string slot);
        ~AP_Character();

        static AP_Character* LoadFromDatabase(Player* player);
        static bool IsSlotBound(const std::string& slot);

        bool Update();
        const Items::ItemsContainer& GetItemsContainer() const;
        Player* GetPlayer();
        std::string GetItemName(int64 itemId) const;
        bool IsZoneUnlocked(uint32 zoneId) const;
        void Teleport(const Items::ZoneItem& zone);

        // PlayerScripts events
        void OnPlayerAchievementComplete(const AchievementEntry* achievement);
        void OnPlayerDied(const std::string& cause);
        void OnPlayerCompleteQuest(const Quest* quest);
        void OnPlayerGiveXP(uint32& xp, Unit* victim, uint8 xpSource);
        void OnPlayerBeforeGetLevelForXPGain(uint8& level);
        void OnPlayerLearnTaxiNode(uint32 nodeId);
        void OnPlayerBeforeLogout();

        // ItemScripts events
        void OnUseArchipelagoStone(Item* item);
        void OnSelectArchipelagoStoneGossip(Item* item, uint32 sender, uint32 action);

    private:
        std::unique_ptr<Network::Client> ap;
        Player* player;
        AP_Stone apStone;
        std::string uuid;
        std::string slot;
        int itemIndex;
        double lastDeathTime;
        bool deathLinkEnabled;
        bool itemsSynced;
        bool run;
        std::list<std::string> tags;
        Items::ItemsContainer items;
        Locations::LocationsContainer locations;
        std::unordered_set<uint32> unlockedZones;
        PlayerPosition lastUnlockedPosition;
        std::chrono::seconds nextLockedZoneCheck;
        std::chrono::seconds nextSave;
        uint32 goalAchievementId;
        uint8 maxLevel;
        uint8 apLevel;
        uint32 apExp;
        uint32 xpForLevel;
        bool goalCompleted;

        AP_Character(Player* player, std::string uuid, std::string slot, int itemIndex, uint8 apLevel, uint32 apExp, bool goalCompleted);

        void CreateAPClient();
        void AddArchipelagoClientHandlers();
        void ConnectAPSlot();

        void SaveToDatabase();
        void SyncLocationChecks();
        void RewardItem(int64_t itemId, bool alreadyRewarded, int sender);
        void CheckIsInLockedZone();
        void SavePosition();
        void LoadXPForLevel();
        void CheckLocation(int32 locationId);

        // AP Handlers
        void APSlotConnectedHandler(const nlohmann::json& data);
        void APSocketErrorHandler(const std::string& error);
        void APSocketDisconnectedHandler();
        void APBouncedHandler(const nlohmann::json& packet);
        void APRoomInfoHandler();
        void APDataPackageHandler(const nlohmann::json& data);
        void APReceivedItemsHandler(const std::list<Network::Client::NetworkItem>& items);
        void APPrintJsonHandler(const std::list<Network::Client::TextNode>& msg);
        void APSlotRefusedHandler(const std::list<std::string>& errors);
        void APMessageErrorHandler(const std::string& error);

        std::string ConvertANSIColoredString(const std::string& str);
        static std::string GenerateUUID();
    };
}

#endif
