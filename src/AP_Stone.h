#ifndef _MOD_ARCHIPELAWOW_AP_STONE_H_
#define _MOD_ARCHIPELAWOW_AP_STONE_H_

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
        bool CanPacketReceive(const WorldPacket& packet);
#endif
        void OnPlayerCreateItem(Item* item);

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
        uint32 gossipIdx;
        uint32 gossipSender;
        uint32 gossipTitleTextId;
        uint32 selectedGlyphItemId; // The glyph item picked in the glyphs menu, for the slots menu
#ifdef MOD_ARCHIPELAWOW_TRANSMOG
        ObjectGuid transmogrifierGuid; // Set while a click or purchase runs on it
        ObjectGuid transmogStoneGuid; // Set from opening mod-transmog's menu until the stone shows its own
        bool transmogVendorShown = false; // Set while mod-transmog's merchant window is the last one sent
        std::chrono::seconds transmogSummonSecond{};
        uint32 transmogSummons = 0;
#endif

        const char* GetZoneTeleportIcon();
        const char* GetDungeonTeleportIcon();
        void SendMainMenu(Item* item);
        void HandleMainMenuAction(Item* item, uint32 action);
        void HandleMailboxAction();
        void HandleHearthstoneAction();
#ifdef MOD_ARCHIPELAWOW_TRANSMOG
        void HandleTransmogAction(Item* item);
        void OpenTransmogMenu();
        static void AddTransmogBackItem(WorldPacket& list);
        static bool IsTransmogEnabled();
        bool RefuseTransmogInCombat();
        bool TakeTransmogSummon();
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
