#ifndef _MOD_ARCHIPELAWOW_AP_STONE_H_
#define _MOD_ARCHIPELAWOW_AP_STONE_H_

#include "Creature.h"
#include "Define.h"
#include "Item.h"
#include "ObjectGuid.h"
#include "Optional.h"
#include "Player.h"
#include "WorldPacket.h"

#include <chrono>
#include <functional>
#include <string>

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
        AP_Character* apCharacter;
        Player* player;
        uint32 gossipIdx;
        uint32 gossipSender;
        uint32 gossipTitleTextId;
#ifdef MOD_ARCHIPELAWOW_TRANSMOG
        ObjectGuid transmogrifierGuid; // Set while a click or purchase runs on it
        ObjectGuid transmogStoneGuid; // Set from opening mod-transmog's menu until the stone shows its own
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
        bool IsTransmogThrottled();
        void ForwardToTransmogrifier(const std::function<bool(Creature*)>& forward);
        bool RunOnTransmogrifier(const std::function<bool(Creature*)>& run);
#endif
        static uint64 CooldownSeconds(uint32 milliseconds);
        Optional<std::string> GetHearthstoneLocation();
        void SendZoneTeleportMenu(Item* item);
        void HandleZoneTeleportSubmenuAction(Item* item, uint32 action);
        void HandleZoneTeleportAction(Item* item, uint32 action);
        void SendDungeonTeleportMenu(Item* item);
        void HandleDungeonTeleportSubmenuAction(Item* item, uint32 action);
        void HandleDungeonTeleportAction(Item* item, uint32 action);
        void HandleGenericTeleportAction(uint32 action);
        void StartGossipMenu(uint32 titleTextId, uint32 sender);
        void SendGossipMenu(Item* item);
        void AddGossipItem(const std::string& icon, const std::string& text, uint32 action);
        void AddGossipItemBack();
        void AddGossipItemBackToMainMenu();
        std::string GossipItemText(const std::string& icon, const std::string& text);
        void SendTeleportsGossipMenu(Item* item, uint32 sender);
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
