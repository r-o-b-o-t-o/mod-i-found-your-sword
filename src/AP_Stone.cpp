#include "AP_Character.h"
#include "AP_Stone.h"
#include "Chat.h"
#include "Define.h"
#include "fmt/core.h"
#include "GossipDef.h"
#include "Item.h"
#include "items/AP_Zones.h"
#include "Player.h"
#include "SharedDefines.h"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

constexpr uint32 AP_STONE_ITEM_ID = 32618;

constexpr uint32 GOSSIP_MENU_MAIN = 0;
constexpr uint32 GOSSIP_ITEM_MAILBOX = 1;
constexpr uint32 GOSSIP_ITEM_TELE_ZONE = 2;
constexpr uint32 GOSSIP_ITEM_TELE_DUNGEON = 3;

constexpr uint32 GOSSIP_MENU_TELE_ZONE = 1;
constexpr uint32 GOSSIP_ITEM_TELE_ZONE_EASTERN_KINGDOMS = 1;
constexpr uint32 GOSSIP_ITEM_TELE_ZONE_KALIMDOR = 2;
constexpr uint32 GOSSIP_ITEM_TELE_ZONE_OUTLAND = 3;
constexpr uint32 GOSSIP_ITEM_TELE_ZONE_NORTHREND = 4;

constexpr uint32 GOSSIP_MENU_TELE_ZONE_EASTERN_KINGDOMS = 2;
constexpr uint32 GOSSIP_MENU_TELE_ZONE_KALIMDOR = 3;
constexpr uint32 GOSSIP_MENU_TELE_ZONE_OUTLAND = 4;
constexpr uint32 GOSSIP_MENU_TELE_ZONE_NORTHREND = 5;

constexpr uint32 GOSSIP_MENU_TELE_DUNGEON = 6;
constexpr uint32 GOSSIP_ITEM_TELE_DUNGEON_CLASSIC = 1;
constexpr uint32 GOSSIP_ITEM_TELE_DUNGEON_OUTLAND = 2;
constexpr uint32 GOSSIP_ITEM_TELE_DUNGEON_NORTHREND = 3;

constexpr uint32 GOSSIP_MENU_TELE_DUNGEON_CLASSIC = 7;
constexpr uint32 GOSSIP_MENU_TELE_DUNGEON_OUTLAND = 8;
constexpr uint32 GOSSIP_MENU_TELE_DUNGEON_NORTHREND = 9;

constexpr uint32 GOSSIP_ITEM_BACK = std::numeric_limits<uint32>::max() - 1;
constexpr uint32 GOSSIP_ITEM_BACK_TO_MAIN = std::numeric_limits<uint32>::max() - 2;

ModArchipelaWoW::AP_Stone::AP_Stone(AP_Character* apCharacter) :
    apCharacter(apCharacter),
    player(apCharacter->GetPlayer()),
    gossipIdx(0),
    gossipSender(0),
    gossipTitleTextId(0)
{
}

void ModArchipelaWoW::AP_Stone::CreateItem()
{
    if (!player)
    {
        return;
    }

    if (player->HasItemCount(AP_STONE_ITEM_ID, 1, true))
    {
        return;
    }

    player->AddItem(AP_STONE_ITEM_ID, 1);
}

void ModArchipelaWoW::AP_Stone::OnUse(Item* item)
{
    SendMainMenu(item);
}

void ModArchipelaWoW::AP_Stone::OnGossipSelect(Item* item, uint32 sender, uint32 action)
{
    if (action == GOSSIP_ITEM_BACK_TO_MAIN)
    {
        SendMainMenu(item);
        return;
    }

    if (sender == GOSSIP_MENU_MAIN)
    {
        HandleMainMenuAction(item, action);
    }
    else if (sender == GOSSIP_MENU_TELE_ZONE)
    {
        HandleZoneTeleportSubmenuAction(item, action);
    }
    else if (sender == GOSSIP_MENU_TELE_ZONE_EASTERN_KINGDOMS || sender == GOSSIP_MENU_TELE_ZONE_KALIMDOR || sender == GOSSIP_MENU_TELE_ZONE_OUTLAND || sender == GOSSIP_MENU_TELE_ZONE_NORTHREND)
    {
        HandleZoneTeleportAction(item, action);
    }
    else if (sender == GOSSIP_MENU_TELE_DUNGEON)
    {
        HandleDungeonTeleportSubmenuAction(item, action);
    }
    else if (sender == GOSSIP_MENU_TELE_DUNGEON_CLASSIC || sender == GOSSIP_MENU_TELE_DUNGEON_OUTLAND || sender == GOSSIP_MENU_TELE_DUNGEON_NORTHREND)
    {
        HandleDungeonTeleportAction(item, action);
    }
}

const char* ModArchipelaWoW::AP_Stone::GetZoneTeleportIcon()
{
    uint8 race = player->getRace(true);

    if (race == RACE_HUMAN) return "Icons/Spell_Arcane_TeleportStormWind";
    if (race == RACE_DWARF || race == RACE_GNOME) return "Icons/Spell_Arcane_TeleportIronForge";
    if (race == RACE_NIGHTELF) return "Icons/Spell_Arcane_TeleportDarnassus";
    if (race == RACE_DRAENEI) return "Icons/Spell_Arcane_TeleportExodar";

    if (race == RACE_ORC || race == RACE_TROLL) return "Icons/Spell_Arcane_TeleportOrgrimmar";
    if (race == RACE_TAUREN) return "Icons/Spell_Arcane_TeleportThunderBluff";
    if (race == RACE_UNDEAD_PLAYER) return "Icons/Spell_Arcane_TeleportUnderCity";
    if (race == RACE_BLOODELF) return "Icons/Spell_Arcane_TeleportSilvermoon";

    return "Icons/Spell_Arcane_TeleportDalaran";
}

const char* ModArchipelaWoW::AP_Stone::GetDungeonTeleportIcon()
{
    return player->GetTeamId(true) == TEAM_ALLIANCE
        ? "Icons/Achievement_Boss_EdwinVancleef"
        : "Icons/Spell_Shadow_SummonFelGuard";
}

void ModArchipelaWoW::AP_Stone::SendMainMenu(Item* item)
{
    StartGossipMenu(1, GOSSIP_MENU_MAIN);
    AddGossipItem("Icons/INV_Letter_03", "Open Mailbox", GOSSIP_ITEM_MAILBOX);
    if (HasAnyZoneUnlocked()) AddGossipItem(GetZoneTeleportIcon(), "Teleport to Zone", GOSSIP_ITEM_TELE_ZONE);
    if (HasAnyDungeonUnlocked()) AddGossipItem(GetDungeonTeleportIcon(), "Teleport to Dungeon", GOSSIP_ITEM_TELE_DUNGEON);
    SendGossipMenu(item);
}

void ModArchipelaWoW::AP_Stone::HandleMainMenuAction(Item* item, uint32 action)
{
    if (action == GOSSIP_ITEM_MAILBOX) HandleMailboxAction();
    else if (action == GOSSIP_ITEM_TELE_ZONE) SendZoneTeleportMenu(item);
    else if (action == GOSSIP_ITEM_TELE_DUNGEON) SendDungeonTeleportMenu(item);
}

void ModArchipelaWoW::AP_Stone::HandleMailboxAction()
{
    player->PlayerTalkClass->SendCloseGossip();

    if (player->IsInCombat())
    {
        ChatHandler(player->GetSession()).SendSysMessage("|cFFFF0000Cannot do this while in combat.");
        return;
    }

    player->GetSession()->SendShowMailBox(player->GetGUID());
}

void ModArchipelaWoW::AP_Stone::SendZoneTeleportMenu(Item* item)
{
    StartGossipMenu(1, GOSSIP_MENU_TELE_ZONE);
    if (HasAnyKalimdorZoneUnlocked()) AddGossipItem("Icons/Achievement_Zone_Kalimdor_01", "Kalimdor", GOSSIP_ITEM_TELE_ZONE_KALIMDOR);
    if (HasAnyEasternKingdomsZoneUnlocked()) AddGossipItem("Icons/Achievement_Zone_EasternKingdoms_01", "Eastern Kingdoms", GOSSIP_ITEM_TELE_ZONE_EASTERN_KINGDOMS);
    if (HasAnyOutlandZoneUnlocked()) AddGossipItem("Icons/Achievement_Zone_Outland_01", "Outland", GOSSIP_ITEM_TELE_ZONE_OUTLAND);
    if (HasAnyNorthrendZoneUnlocked()) AddGossipItem("Icons/Achievement_Zone_Northrend_01", "Northrend", GOSSIP_ITEM_TELE_ZONE_NORTHREND);
    AddGossipItemBack();
    SendGossipMenu(item);
}

void ModArchipelaWoW::AP_Stone::HandleZoneTeleportSubmenuAction(Item* item, uint32 action)
{
    if (action == GOSSIP_ITEM_TELE_ZONE_EASTERN_KINGDOMS) SendTeleportsGossipMenu(item, GOSSIP_MENU_TELE_ZONE_EASTERN_KINGDOMS);
    else if (action == GOSSIP_ITEM_TELE_ZONE_KALIMDOR) SendTeleportsGossipMenu(item, GOSSIP_MENU_TELE_ZONE_KALIMDOR);
    else if (action == GOSSIP_ITEM_TELE_ZONE_OUTLAND) SendTeleportsGossipMenu(item, GOSSIP_MENU_TELE_ZONE_OUTLAND);
    else if (action == GOSSIP_ITEM_TELE_ZONE_NORTHREND) SendTeleportsGossipMenu(item, GOSSIP_MENU_TELE_ZONE_NORTHREND);
    else if (action == GOSSIP_ITEM_BACK) SendMainMenu(item);
}

void ModArchipelaWoW::AP_Stone::HandleZoneTeleportAction(Item* item, uint32 action)
{
    if (action == GOSSIP_ITEM_BACK) SendZoneTeleportMenu(item);
    else HandleGenericTeleportAction(item, action);
}

void ModArchipelaWoW::AP_Stone::SendDungeonTeleportMenu(Item* item)
{
    StartGossipMenu(1, GOSSIP_MENU_TELE_DUNGEON);
    if (HasAnyClassicDungeonUnlocked()) AddGossipItem("Icons/Spell_Holy_ReviveChampion", "Classic", GOSSIP_ITEM_TELE_DUNGEON_CLASSIC);
    if (HasAnyOutlandDungeonUnlocked()) AddGossipItem("Icons/Spell_Holy_SummonChampion", "The Burning Crusade", GOSSIP_ITEM_TELE_DUNGEON_OUTLAND);
    if (HasAnyNorthrendDungeonUnlocked()) AddGossipItem("Icons/Spell_Holy_ChampionsBond", "Wrath of the Lich King", GOSSIP_ITEM_TELE_DUNGEON_NORTHREND);
    AddGossipItemBack();
    SendGossipMenu(item);
}

void ModArchipelaWoW::AP_Stone::HandleDungeonTeleportSubmenuAction(Item* item, uint32 action)
{
    if (action == GOSSIP_ITEM_TELE_DUNGEON_CLASSIC) SendTeleportsGossipMenu(item, GOSSIP_MENU_TELE_DUNGEON_CLASSIC);
    else if (action == GOSSIP_ITEM_TELE_DUNGEON_OUTLAND) SendTeleportsGossipMenu(item, GOSSIP_MENU_TELE_DUNGEON_OUTLAND);
    else if (action == GOSSIP_ITEM_TELE_DUNGEON_NORTHREND) SendTeleportsGossipMenu(item, GOSSIP_MENU_TELE_DUNGEON_NORTHREND);
    else if (action == GOSSIP_ITEM_BACK) SendMainMenu(item);
}

void ModArchipelaWoW::AP_Stone::HandleDungeonTeleportAction(Item* item, uint32 action)
{
    if (action == GOSSIP_ITEM_BACK) SendDungeonTeleportMenu(item);
    else HandleGenericTeleportAction(item, action);
}

void ModArchipelaWoW::AP_Stone::HandleGenericTeleportAction(Item* item, uint32 action)
{
    player->PlayerTalkClass->SendCloseGossip();

    if (player->IsInCombat())
    {
        ChatHandler(player->GetSession()).SendSysMessage("|cFFFF0000Cannot do this while in combat.");
        return;
    }

    auto zone = apCharacter->GetItemsContainer().zones.GetZone(action);
    if (zone.has_value())
    {
        apCharacter->Teleport(zone.value());
    }
}

void ModArchipelaWoW::AP_Stone::StartGossipMenu(uint32 titleTextId, uint32 sender)
{
    player->PlayerTalkClass->ClearMenus();
    gossipIdx = 0;
    gossipSender = sender;
    gossipTitleTextId = titleTextId;
}

void ModArchipelaWoW::AP_Stone::SendGossipMenu(Item* item)
{
    player->PlayerTalkClass->SendGossipMenu(gossipTitleTextId, item->GetGUID());
}

void ModArchipelaWoW::AP_Stone::AddGossipItem(const std::string& icon, const std::string& text, uint32 action)
{
    player->PlayerTalkClass->GetGossipMenu().AddMenuItem(gossipIdx++, GOSSIP_ICON_CHAT, GossipItemText(icon, text), gossipSender, action, "", 0);
}

void ModArchipelaWoW::AP_Stone::AddGossipItemBack()
{
    AddGossipItem("PaperDollInfoFrame/UI-GearManager-Undo", "Back", GOSSIP_ITEM_BACK);
}

void ModArchipelaWoW::AP_Stone::AddGossipItemBackToMainMenu()
{
    AddGossipItem("PaperDollInfoFrame/UI-GearManager-LeaveItem-Opaque", "Back to main menu", GOSSIP_ITEM_BACK_TO_MAIN);
}

std::string ModArchipelaWoW::AP_Stone::GossipItemText(const std::string& icon, const std::string& text)
{
    return fmt::format("|TInterface/{}:32:32:12:-1|t    {}", icon, text);
}

void ModArchipelaWoW::AP_Stone::SendTeleportsGossipMenu(Item* item, uint32 sender)
{
    struct TeleportGossipItem
    {
    public:
        int64 itemId;
        std::string text;
        Items::ZoneItem zone;

        TeleportGossipItem(int64 itemId, const std::string& text, const Items::ZoneItem& zone) :
            itemId(itemId),
            text(text),
            zone(zone)
        {
        }
    };

    std::vector<TeleportGossipItem> items;
    for (auto it = apCharacter->GetItemsContainer().zones.Begin(); it != apCharacter->GetItemsContainer().zones.End(); ++it)
    {
        int64 itemId = it->first;
        auto& zone = it->second;
        items.emplace_back(itemId, apCharacter->GetItemName(itemId), zone);
    }
    std::ranges::sort(items, [](const TeleportGossipItem& a, const TeleportGossipItem& b)
    {
        return a.text < b.text;
    });

    StartGossipMenu(1, sender);
    for (auto& item : items)
    {
        if (item.zone.gossipMenu == gossipSender && apCharacter->IsZoneUnlocked(item.zone.id))
        {
            AddGossipItem(item.zone.icon, item.text, item.itemId);
        }
    }
    AddGossipItemBack();
    AddGossipItemBackToMainMenu();
    SendGossipMenu(item);
}

bool ModArchipelaWoW::AP_Stone::HasAnyEasternKingdomsZoneUnlocked()
{
    return HasAnyZoneItemUnlocked(GOSSIP_MENU_TELE_ZONE_EASTERN_KINGDOMS);
}

bool ModArchipelaWoW::AP_Stone::HasAnyKalimdorZoneUnlocked()
{
    return HasAnyZoneItemUnlocked(GOSSIP_MENU_TELE_ZONE_KALIMDOR);
}

bool ModArchipelaWoW::AP_Stone::HasAnyOutlandZoneUnlocked()
{
    return HasAnyZoneItemUnlocked(GOSSIP_MENU_TELE_ZONE_OUTLAND);
}

bool ModArchipelaWoW::AP_Stone::HasAnyNorthrendZoneUnlocked()
{
    return HasAnyZoneItemUnlocked(GOSSIP_MENU_TELE_ZONE_NORTHREND);
}

bool ModArchipelaWoW::AP_Stone::HasAnyZoneUnlocked()
{
    return HasAnyEasternKingdomsZoneUnlocked() || HasAnyKalimdorZoneUnlocked() || HasAnyOutlandZoneUnlocked() || HasAnyNorthrendZoneUnlocked();
}

bool ModArchipelaWoW::AP_Stone::HasAnyClassicDungeonUnlocked()
{
    return HasAnyZoneItemUnlocked(GOSSIP_MENU_TELE_DUNGEON_CLASSIC);
}

bool ModArchipelaWoW::AP_Stone::HasAnyOutlandDungeonUnlocked()
{
    return HasAnyZoneItemUnlocked(GOSSIP_MENU_TELE_DUNGEON_OUTLAND);
}

bool ModArchipelaWoW::AP_Stone::HasAnyNorthrendDungeonUnlocked()
{
    return HasAnyZoneItemUnlocked(GOSSIP_MENU_TELE_DUNGEON_NORTHREND);
}

bool ModArchipelaWoW::AP_Stone::HasAnyDungeonUnlocked()
{
    return HasAnyClassicDungeonUnlocked() || HasAnyOutlandDungeonUnlocked() || HasAnyNorthrendDungeonUnlocked();
}

bool ModArchipelaWoW::AP_Stone::HasAnyZoneItemUnlocked(uint32 menu)
{
    for (auto it = apCharacter->GetItemsContainer().zones.Begin(); it != apCharacter->GetItemsContainer().zones.End(); ++it)
    {
        if (it->second.gossipMenu == menu && apCharacter->IsZoneUnlocked(it->second.id))
        {
            return true;
        }
    }

    return false;
}
