#ifndef _MOD_ARCHIPELAWOW_AP_STONE_H_
#define _MOD_ARCHIPELAWOW_AP_STONE_H_

#include "AP_StoneShop.h"
#include "Creature.h"
#include "DBCStructure.h"
#include "Define.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "ObjectGuid.h"
#include "Optional.h"
#include "Player.h"
#include "SpellInfo.h"
#include "WorldPacket.h"

#include <chrono>
#include <functional>
#include <list>
#include <string>
#include <vector>

namespace ModArchipelaWoW
{
    class AP_Character;

    class AP_Stone
    {
    public:
        AP_Stone(AP_Character* apCharacter);

        void CreateItem();
        void OnUse(Item* item);
        void OnGossipSelect(Item* item, uint32 sender, uint32 action);
#ifdef MOD_ARCHIPELAWOW_TRANSMOG
        void OnGossipSelectCode(uint32 sender, uint32 action, const char* code);
        bool CanPacketSend(const WorldPacket& packet);
#endif
        bool CanPacketReceive(const WorldPacket& packet);
        void OnPlayerCreateItem(Item* item);
        void OnLocationInfo(const std::list<Network::Client::NetworkItem>& items);
        /// Closes the shop's merchant window, if it is open, with the main menu in its place.
        void OnShopSoldOut();

    private:
        struct GlyphSpell
        {
            const SpellInfo* spell;
            const GlyphPropertiesEntry* glyph;
        };

        struct AvailableGlyph
        {
            uint32 itemId;
            std::string name;
            std::string icon;
        };

        AP_Character* apCharacter;
        Player* player;
        AP_StoneShop shop;
        uint32 gossipIdx;
        uint32 gossipSender;
        uint32 gossipTitleTextId;
        uint32 selectedGlyphItemId; // The glyph item picked in the glyphs menu, for the slots menu
        uint8 selectedHeirloomWeaponSlot; // The slot picked in the heirloom weapon slots menu, for the weapon list
        bool mailboxShown = false; // Set once the stone has opened the mailbox
        std::chrono::seconds summonSecond{};
        uint32 summons = 0;
#ifdef MOD_ARCHIPELAWOW_TRANSMOG
        ObjectGuid transmogrifierGuid; // Set while a click or purchase runs on it
        ObjectGuid transmogStoneGuid; // Set from opening mod-transmog's menu until the stone shows its own
        bool transmogVendorShown = false; // Set while mod-transmog's merchant window is the last one sent
#endif

        const char* GetZoneTeleportIcon();
        const char* GetDungeonTeleportIcon();
        void SendMainMenu(Item* item);
        void HandleMainMenuAction(Item* item, uint32 action);
        void HandleMailboxAction();
        bool CanMailPacketReceive(const WorldPacket& packet);
        bool TakeSummon();
        void HandleHearthstoneAction();
        void HandleShopAction();
#ifdef MOD_ARCHIPELAWOW_TRANSMOG
        bool CanTransmogPacketReceive(const WorldPacket& packet);
        void HandleTransmogAction(Item* item);
        void OpenTransmogMenu();
        static void AddTransmogBackItem(WorldPacket& list);
        static bool IsTransmogEnabled();
        bool RefuseTransmogInCombat();
        void ForwardToTransmogrifier(const std::function<bool(Creature*)>& forward);
        bool RunOnTransmogrifier(const std::function<bool(Creature*)>& run);
#endif
        void DismountForItemCast();
        static uint64 CooldownSeconds(uint32 milliseconds);
        Optional<std::string> GetHearthstoneLocation();
        void SendZoneTeleportMenu(Item* item);
        void HandleZoneTeleportSubmenuAction(Item* item, uint32 action);
        void HandleZoneTeleportAction(Item* item, uint32 action);
        void SendDungeonTeleportMenu(Item* item);
        void HandleDungeonTeleportSubmenuAction(Item* item, uint32 action);
        void HandleDungeonTeleportAction(Item* item, uint32 action);
        void HandleGenericTeleportAction(uint32 action);
        bool RefuseTeleport();
        void SendFriendTeleportMenu(Item* item);
        void HandleFriendTeleportAction(Item* item, uint32 action);
        bool RefuseFriendTeleportOnCooldown();
        std::vector<Player*> GetTeleportableFriends();
        static std::string GetCharacterIcon(Player* target);
        bool HasAnyGlyphAvailable();
        bool HasAnyGlyphAvailable(uint32 type);
        void SendGlyphsMenu(Item* item);
        void HandleGlyphsAction(Item* item, uint32 action);
        void SendGlyphListMenu(Item* item, uint32 type);
        void HandleGlyphListAction(Item* item, uint32 action);
        void SendGlyphSlotsMenu(Item* item);
        void HandleGlyphSlotAction(Item* item, uint32 action);
        static Optional<GlyphSpell> GetGlyphSpell(uint32 itemId);
        Optional<GlyphSpell> GetAvailableGlyphSpell(uint32 itemId);
        std::vector<AvailableGlyph> GetAvailableGlyphs(uint32 type);
        std::string GetGlyphItemName(uint32 itemId);
        std::string GetInscribedGlyphName(const GlyphPropertiesEntry* glyph);
        static std::string StripGlyphPrefix(std::string name);
        std::string GetGlyphIcon(uint32 type);
        bool HasAnyPreference();
        bool HasHeirloomWeaponChoice();
        void SendPreferencesMenu(Item* item);
        void HandlePreferencesAction(Item* item, uint32 action);
        void SendHeirloomWeaponSlotsMenu(Item* item);
        void HandleHeirloomWeaponSlotAction(Item* item, uint32 action);
        void SendHeirloomWeaponListMenu(Item* item);
        void HandleHeirloomWeaponListAction(Item* item, uint32 action);
        std::string GetItemName(uint32 itemId);
        static std::string GetItemIcon(uint32 itemId);
        static std::string GetItemIcon(const ItemTemplate* itemTemplate);
        void StartGossipMenu(uint32 titleTextId, uint32 sender);
        void SendGossipMenu(Item* item);
        void AddGossipItem(const std::string& icon, const std::string& text, uint32 action);
        void AddGossipItemBack();
        void AddGossipItemBackToMainMenu();
        std::string GossipItemText(const std::string& icon, const std::string& text);
        void SendTeleportsGossipMenu(Item* item, uint32 sender, uint32 titleTextId);
        bool HasAnyEasternKingdomsZoneUnlocked();
        bool HasAnyKalimdorZoneUnlocked();
        bool HasAnyOutlandZoneUnlocked();
        bool HasAnyNorthrendZoneUnlocked();
        bool HasAnyZoneUnlocked();
        bool HasAnyClassicDungeonUnlocked();
        bool HasAnyOutlandDungeonUnlocked();
        bool HasAnyNorthrendDungeonUnlocked();
        bool HasAnyDungeonUnlocked();
        bool HasAnyZoneItemUnlocked(uint32 menu);
    };
}

#endif
