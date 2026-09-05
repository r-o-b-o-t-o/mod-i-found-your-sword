#ifndef _MOD_ARCHIPELAWOW_AP_CHARACTER_H_
#define _MOD_ARCHIPELAWOW_AP_CHARACTER_H_

#include "AP_PlayerPosition.h"
#include "AP_Stone.h"
#include "Creature.h"
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
#include "NPCPackets.h"
#include "Player.h"
#include "QuestDef.h"
#include "Trainer.h"
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
        void SayToArchipelago(const std::string& text);

        // PlayerScripts events
        void OnPlayerAchievementComplete(const AchievementEntry* achievement);
        void OnPlayerDied(const std::string& cause);
        void OnPlayerCompleteQuest(const Quest* quest);
        void OnPlayerGiveXP(uint32& xp, Unit* victim, uint8 xpSource);
        void OnPlayerBeforeGetLevelForXPGain(uint8& level);
        void OnPlayerLearnTaxiNode(uint32 nodeId);
        void OnPlayerAfterTakeItemFromMail(uint32 wowItemId);
        bool OnPlayerCanLearnSpell(uint32 spellId);
        void OnPlayerGetTrainerSpellState(uint32 spellId, Trainer::SpellState& state);
        void OnPlayerBeforeReceiveSpellListFromTrainer(WorldPackets::NPC::TrainerList& trainerList);
        void OnPlayerAfterTrainSpell(Creature* trainer, uint32 spellId);
        void ReopenTrainerWindow(Creature* trainer);
        void OnPlayerCreateItem(Item* item);
        void OnPlayerBeforeLogout();
        bool OnPlayerChat(uint32 type, const std::string& msg, const std::string& channelName);

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
        std::unordered_set<uint32> grantedSpells;
        std::unordered_set<int32> checkedLocations;
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
        /// The spells the module is in the middle of handing over, which the block that keeps a
        /// randomized spell out of the character's hands stands aside for. A set rather than a flag
        /// because a wrapper teaches several spells at once, and the ones with an Archipelago item
        /// of their own have to stay blocked while the rest go through.
        std::unordered_set<uint32> grantingSpells;
        /// The messages this character relayed to the room and has already read in its own chat
        /// box, waiting for the room to echo them back so the copy can be dropped.
        std::list<std::string> pendingChatEchoes;

        AP_Character(Player* player, std::string uuid, std::string slot, int itemIndex, uint8 apLevel, uint32 apExp, bool goalCompleted);

        void AnnounceXPGain(uint32 baseXp, uint32 totalXp, uint32 bonusPct) const;
        uint32 GetProgressiveStep(Items::ProgressiveType type) const;
        void ApplyMovementSpeedBonus();
        void RemoveOutgrownProgressiveItems(Items::ProgressiveType type);
        void GrantSpell(uint32 spellId);
        void RemoveUngrantedSpells();
        void RemoveSpellIfUngranted(uint32 spellId);

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
        void RelayChatToArchipelago(const std::string& text);
        void SendCommandToArchipelago(const std::string& command);

        // AP Handlers
        void APSlotConnectedHandler(const nlohmann::json& data);
        void APSocketErrorHandler(const std::string& error);
        void APSocketDisconnectedHandler();
        void APBouncedHandler(const nlohmann::json& packet);
        void APRoomInfoHandler();
        void APDataPackageHandler(const nlohmann::json& data);
        void APReceivedItemsHandler(const std::list<Network::Client::NetworkItem>& items);
        void APPrintJsonHandler(const Network::Client::PrintJson& msg);
        void APSlotRefusedHandler(const std::list<std::string>& errors);
        void APMessageErrorHandler(const std::string& error);

        static std::string ConvertANSIColoredString(const std::string& str);
        static std::string GenerateUUID();
    };
}

#endif
