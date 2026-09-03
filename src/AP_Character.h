#ifndef _MOD_ARCHIPELAWOW_AP_CHARACTER_H_
#define _MOD_ARCHIPELAWOW_AP_CHARACTER_H_

#include "AP_PlayerPosition.h"
#include "AP_Stone.h"
#include "DBCStructure.h"
#include "Define.h"
#include "Item.h"
#include "items/AP_ItemsContainer.h"
#include "items/AP_Progressive.h"
#include "items/AP_Zones.h"
#include "locations/AP_LocationsContainer.h"
#include "Mail.h"
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
#include <unordered_map>
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
        uint32 GetGoldPouchAmount() const;
        void SendLevelReport() const;
        bool IsSlotConnected() const;

        // PlayerScripts events
        void OnPlayerAchievementComplete(const AchievementEntry* achievement);
        void OnPlayerDied(const std::string& cause);
        void OnPlayerCompleteQuest(const Quest* quest);
        void OnPlayerGiveXP(uint32& xp, Unit* victim, uint8 xpSource);
        void OnPlayerBeforeGetLevelForXPGain(uint8& level);
        void OnPlayerLearnTaxiNode(uint32 nodeId);
        void OnPlayerAfterTakeItemFromMail(uint32 wowItemId);
        void OnPlayerCreateItem(Item* item);
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
        int countedItemIndex;
        double lastDeathTime;
        bool deathLinkEnabled;
        bool itemsSynced;
        bool run;
        std::list<std::string> tags;
        Items::ItemsContainer items;
        Locations::LocationsContainer locations;
        std::unordered_set<uint32> unlockedZones;
        std::unordered_map<Items::ProgressiveType, uint32> progressiveCounts;
        PlayerPosition lastUnlockedPosition;
        std::chrono::seconds nextLockedZoneCheck;
        std::chrono::seconds nextSave;
        uint32 goalAchievementId;
        uint8 gearRewardLevelWindow;
        bool gearAllArmorTypes;
        uint8 maxLevel;
        uint8 apLevel;
        uint32 apExp;
        uint32 xpForLevel;
        bool goalCompleted;

        AP_Character(Player* player, std::string uuid, std::string slot, int itemIndex, uint8 apLevel, uint32 apExp, bool goalCompleted);

        void AnnounceXPGain(uint32 baseXp, uint32 totalXp, uint32 bonusPct) const;
        uint32 GetProgressiveStep(Items::ProgressiveType type) const;
        void ApplyMovementSpeedBonus();
        void RemoveOutgrownProgressiveItems(Items::ProgressiveType type);

        void CreateAPClient();
        void AddArchipelagoClientHandlers();
        void ConnectAPSlot();

        void SaveToDatabase();
        void SyncLocationChecks();
        void RewardItem(int64_t itemId, bool alreadyRewarded, bool alreadyCounted, int sender);
        void MailItemReward(uint32 wowItemId, int64_t apItemId, int sender);
        void CheckIsInLockedZone();
        void SavePosition();
        void LoadXPForLevel();
        void CheckLocation(int32 locationId);
        MailSender GetMailSender(int sender);

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

        static std::string ConvertANSIColoredString(const std::string& str);
        static std::string GenerateUUID();
    };
}

#endif
