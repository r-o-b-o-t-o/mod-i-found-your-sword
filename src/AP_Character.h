#ifndef _MOD_ARCHIPELAWOW_AP_CHARACTER_H_
#define _MOD_ARCHIPELAWOW_AP_CHARACTER_H_

#include "AP_PlayerPosition.h"
#include "AP_Stone.h"
#include "AP_Traps.h"
#include "Creature.h"
#include "DBCStructure.h"
#include "Define.h"
#include "Item.h"
#include "items/AP_ItemsContainer.h"
#include "items/AP_Progressive.h"
#include "items/AP_Traps.h"
#include "items/AP_Zones.h"
#include "locations/AP_LocationsContainer.h"
#include "Mail.h"
#include "network/AP_Client.h"
#include "nlohmann/json.hpp"
#include "NPCPackets.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "QuestDef.h"
#include "Trainer.h"
#include "Unit.h"
#include "WorldPacket.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <list>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
        /// Whether the seed keeps the character out of the zone until its item arrives.
        bool IsZoneLocked(uint32 zoneId) const;
        bool IsFriendTeleportUnlocked() const;
        const std::unordered_set<uint32>& GetReceivedGlyphs() const;

        // Preferences menu of the Archipelago Stone
        /// Paladins, shamans and druids, which can play either as casters or in melee.
        bool IsHybrid() const;
        bool PrefersCasterHeirlooms() const;
        void ToggleCasterHeirlooms();
        bool PrefersAllArmorTypes() const;
        void ToggleAllArmorTypes();
        bool PrefersCasterWeapons() const;
        void ToggleCasterWeapons();
        /// Whether random weapon rewards may be held-in-off-hand items, which only casters and
        /// hunters get any use of.
        bool WantsHeldOffHands() const;
        /// The level the run ends at, 0 until the slot data has arrived.
        uint8 GetMaxLevel() const;
        bool PrefersCasterWorldBuffs() const;
        void ToggleCasterWorldBuffs();
        /// The weapon in an heirloom weapon slot, 0 for none yet.
        uint32 GetHeirloomWeapon(uint8 slot) const;
        /// Fills an heirloom weapon slot, and sends its weapon right away if it has already arrived.
        void SetHeirloomWeapon(uint8 slot, uint32 itemId);
        /// Whether the slot's weapon was sent, after which it can no longer be changed.
        bool IsHeirloomWeaponSent(uint8 slot) const;
        /// Heirloom Weapons that arrived before the player filled their slot.
        uint32 GetPendingHeirloomWeapons() const;
        void Teleport(const Items::ZoneItem& zone);
        bool TeleportToPlayer(Player* target);
        uint32 GetGoldPouchAmount() const;
        void SendLevelReport() const;
        void SendExperienceBar() const;
        bool IsSlotConnected() const;
        void SayToArchipelago(const std::string& text);

        // PlayerScripts events
        void OnPlayerAchievementComplete(const AchievementEntry* achievement);
        void OnPlayerCriteriaProgress(const AchievementCriteriaEntry* criteria);
        void OnPlayerDied(const std::string& cause);
        void OnPlayerCompleteQuest(const Quest* quest);
        void OnPlayerGiveXP(uint32& xp, Unit* victim, uint8 xpSource);
        void OnPlayerBeforeGetLevelForXPGain(uint8& level);
        void OnPlayerLevelChanged();
        void OnPlayerSendInitialPacketsBeforeAddToMap();
        void OnPlayerLearnTaxiNode(uint32 nodeId);
        void OnPlayerTextEmote(uint32 textEmote, ObjectGuid target);
        void OnPlayerAfterTakeItemFromMail(uint32 wowItemId);
        bool OnPlayerCanLearnSpell(uint32 spellId);
        void OnPlayerGetTrainerSpellState(uint32 spellId, Trainer::SpellState& state);
        void OnPlayerBeforeReceiveSpellListFromTrainer(WorldPackets::NPC::TrainerList& trainerList);
        void OnPlayerAfterTrainSpell(Creature* trainer, uint32 spellId);
        void OnPlayerUpdateSkill(uint32 skillId, uint32 newValue);
        void ReopenTrainerWindow(Creature* trainer);
        void OnPlayerCreateItem(Item* item);
        void OnPlayerBeforeLogout();
        bool OnPlayerChat(uint32 type, const std::string& msg, const std::string& channelName);
        void OnPlayerSpellCast(const Spell* spell);
        void OnPlayerResurrect();

        // GlobalScripts events
        void OnDungeonEncounterCredited(uint32 encounterId);

        // ItemScripts events
        void OnUseArchipelagoStone(Item* item);
        void OnSelectArchipelagoStoneGossip(Item* item, uint32 sender, uint32 action);
        /// Whether one of the module's consumables can be used now, telling the character why not
        /// when it cannot.
        bool CanUseConsumable(uint32 itemId);
#ifdef MOD_ARCHIPELAWOW_TRANSMOG
        void OnSelectArchipelagoStoneGossipCode(uint32 sender, uint32 action, const char* code);

        // ServerScripts events
        bool CanPacketSend(const WorldPacket& packet);
        bool CanPacketReceive(const WorldPacket& packet);
#endif

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
        /// The glyph items this slot has received, which the stone can inscribe again at any time.
        std::unordered_set<uint32> receivedGlyphs;
        std::unordered_set<int32> checkedLocations;
        /// This world's locations a hint already gives away, which a Scroll of Divination skips.
        std::unordered_set<int64> hintedLocations;
        std::unordered_map<Items::ProgressiveType, uint32> progressiveCounts;
        PlayerPosition lastUnlockedPosition;
        std::chrono::seconds nextLockedZoneCheck;
        std::chrono::seconds nextSave;
        uint32 goalAchievementId;
        uint8 gearRewardLevelWindow;
        bool casterHeirlooms;
        bool allArmorTypes;
        bool casterWorldBuffs;
        bool casterWeapons;
        /// Random World Buffs received while dead, applied once the character is back to life.
        uint8 pendingWorldBuffs;
        /// Traps received while they could not spring, in the order they arrived.
        std::vector<Items::TrapType> pendingTraps;
        /// Game time until which the pending traps wait, after a teleport.
        std::chrono::milliseconds trapsHeldUntil;
        /// The Bombs Trap going off, if one is.
        Optional<Traps::BombRun> bombRun;
        std::array<uint32, Items::MAX_HEIRLOOM_WEAPON_SLOTS> heirloomWeapons;
        /// Persisted, unlike the senders below: the slots are sent in order, so this is also the
        /// number of slots that can no longer be changed.
        uint8 heirloomWeaponsSent;
        /// Who sent each Heirloom Weapon received so far, in order, so its size is the received count.
        /// Rebuilt from the item list the server replays on every connection, like progressiveCounts.
        std::vector<int> heirloomWeaponSenders;
        uint8 maxLevel;
        uint8 apLevel;
        uint32 apExp;
        uint32 xpForLevel;
        bool goalCompleted;
        bool friendTeleportUnlocked;
        bool missedProgressCaughtUp;
        /// Set when the core is about to overwrite the client's experience bar, so Update sends
        /// the Archipelago one again once the core's own update has gone out.
        bool experienceBarStale;
        /// The spell the module is in the middle of handing over, which the block that keeps a
        /// randomized spell out of the character's hands stands aside for. Zero outside a hand-over.
        uint32 grantingSpell;
        /// The messages this character relayed to the room and has already read in its own chat
        /// box, waiting for the room to echo them back so the copy can be dropped.
        std::list<std::string> pendingChatEchoes;

        AP_Character(Player* player, std::string uuid, std::string slot, int itemIndex, uint8 apLevel, uint32 apExp, bool goalCompleted);

        void GivePetXP(uint32 xp, uint8 xpSource) const;
        void GiveArchipelagoExperience(uint32 xp, Unit* victim);
        Player* GetSoulstoneTarget() const;
        void ApplySoulstone(Player* target);
        void ApplyWorldBuff();
        void ApplyPendingWorldBuffs();
        void ReceiveTrap(Items::TrapType type);
        void SpringTrap(Items::TrapType type);
        void SpringPendingTraps();
        uint32 GetProgressiveStep(Items::ProgressiveType type) const;
        void ApplyMovementSpeedBonus();
        void RemoveOutgrownProgressiveItems(Items::ProgressiveType type);
        void GrantSpell(uint32 spellId);
        void LearnGrantedSpell(uint32 spellId);
        void RemoveUngrantedSpells();
        void RemoveSpellIfUngranted(uint32 spellId);

        void CreateAPClient();
        void AddArchipelagoClientHandlers();
        void ConnectAPSlot();

        void SaveToDatabase();
        void LoadPreferences();
        void SavePreferences();
        void SendPendingHeirloomWeapons();
        void SyncLocationChecks();
        void CatchUpMissedProgress();
        void CompleteGoal();
        void RewardItem(int64_t itemId, bool alreadyRewarded, bool alreadyCounted, int sender);
        void MailItemReward(uint32 wowItemId, int64_t apItemId, int sender);
        void CheckIsInLockedZone();
        void SavePosition();
        void InitExperience();
        void LoadXPForLevel();
        void CheckLocation(int32 locationId);
        void CheckLocations(const std::list<int64>& locationIds);
        MailSender GetMailSender(int sender);
        void RelayChatToArchipelago(const std::string& text);
        void SendCommandToArchipelago(const std::string& command);
        std::vector<int64> GetUnhintedLocations() const;
        std::string GetHintsKey() const;

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
        void APDataStorageHandler(const std::string& key, const nlohmann::json& value);

        static std::string ConvertANSIColoredString(const std::string& str);
        static std::string GenerateUUID();
    };
}

#endif
