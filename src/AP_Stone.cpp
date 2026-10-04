#include "AP_Character.h"
#include "AP_Stone.h"
#include "Chat.h"
#include "Common.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "Define.h"
#include "fmt/core.h"
#include "GossipDef.h"
#include "Item.h"
#include "items/AP_Zones.h"
#include "ObjectAccessor.h"
#include "Optional.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "TemporarySummon.h"
#include "Util.h"

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

constexpr uint32 AP_STONE_ITEM_ID = 32618;
constexpr uint32 HEARTHSTONE_ITEM_ID = 6948;
constexpr uint32 HEARTHSTONE_SPELL_ID = 8690;

constexpr uint32 GOSSIP_MENU_MAIN = 0;
constexpr uint32 GOSSIP_ITEM_MAILBOX = 1;
constexpr uint32 GOSSIP_ITEM_TELE_ZONE = 2;
constexpr uint32 GOSSIP_ITEM_TELE_DUNGEON = 3;
constexpr uint32 GOSSIP_ITEM_HEARTHSTONE = 4;

#ifdef MOD_ARCHIPELAWOW_TRANSMOG
constexpr uint32 GOSSIP_ITEM_TRANSMOG = 5;
constexpr uint32 TRANSMOGRIFIER_CREATURE_ID = 190010; // mod-transmog's Warpweaver
constexpr uint32 TRANSMOGRIFIER_LIFETIME = 10 * MINUTE * IN_MILLISECONDS;
constexpr uint32 INVISIBLE_DISPLAY_ID = 11686;
#endif

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

namespace ModArchipelaWoW
{
    AP_Stone::AP_Stone(AP_Character* apCharacter) :
        apCharacter(apCharacter),
        player(apCharacter->GetPlayer()),
        gossipIdx(0),
        gossipSender(0),
        gossipTitleTextId(0)
    {
    }

    void AP_Stone::CreateItem()
    {
        if (!player)
        {
            return;
        }

        if (player->HasItemCount(AP_STONE_ITEM_ID, 1, true))
        {
            return;
        }

        // The stone absorbs the Hearthstone's use, so the real item is redundant from here on and
        // would only cost a bag slot. Destroying it first also means the swap needs one free slot
        // rather than two.
        //
        // Bags only, deliberately: DestroyItemCount would reach the bank, and destroying a banked
        // Hearthstone frees a bank slot rather than the bag slot AddItem needs. With full bags that
        // would leave the character holding neither item.
        if (player->HasItemCount(HEARTHSTONE_ITEM_ID, 1))
        {
            player->DestroyItemCount(HEARTHSTONE_ITEM_ID, 1, true);
        }

        player->AddItem(AP_STONE_ITEM_ID, 1);
    }

    void AP_Stone::OnUse(Item* item)
    {
        // Everything the menu offers depends on slot data, so opening it beforehand would only show
        // a mailbox and two missing teleport lists.
        if (!apCharacter->IsSlotConnected())
        {
            ChatHandler(player->GetSession()).SendSysMessage("|cFFFF0000The Archipelago Stone is dormant until the slot is connected.");
            return;
        }

        SendMainMenu(item);
    }

    void AP_Stone::OnGossipSelect(Item* item, uint32 sender, uint32 action)
    {
#ifdef MOD_ARCHIPELAWOW_TRANSMOG
        // mod-transmog's senders overlap the stone's own (0 is its head slot), hence the flag.
        if (transmogMenuShown)
        {
            Creature* transmogrifier = GetTransmogrifier();
            if (!transmogrifier || !sScriptMgr->OnGossipSelect(player, transmogrifier, sender, action))
            {
                player->PlayerTalkClass->SendCloseGossip();
            }

            return;
        }
#endif

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

#ifdef MOD_ARCHIPELAWOW_TRANSMOG
    void AP_Stone::OnGossipSelectCode(uint32 sender, uint32 action, const char* code)
    {
        // Only mod-transmog's menus ask for input: an item search or a set name.
        if (!transmogMenuShown)
        {
            return;
        }

        Creature* transmogrifier = GetTransmogrifier();
        if (!transmogrifier || !sScriptMgr->OnGossipSelectCode(player, transmogrifier, sender, action, code))
        {
            player->PlayerTalkClass->SendCloseGossip();
        }
    }

    bool AP_Stone::CanSendGossipMessage(const WorldPacket& packet)
    {
        if (!transmogrifierGuid || packet.read<uint64>(0) != transmogrifierGuid.GetRawValue())
        {
            return true;
        }

        // mod-transmog sends its menus on the transmogrifier. Shown and answered as the stone's instead,
        // they stay open wherever the player goes, and the core hands their clicks to the stone with no
        // range check. The hook gets the packet read-only, so it goes out again as a copy.
        player->PlayerTalkClass->GetGossipMenu().SetSenderGUID(transmogStoneGuid);
        WorldPacket redirected(packet);
        redirected.put<uint64>(0, transmogStoneGuid.GetRawValue());
        player->SendDirectMessage(&redirected);
        return false;
    }
#endif

    void AP_Stone::OnPlayerCreateItem(Item* item)
    {
        if (!item || item->GetEntry() != HEARTHSTONE_ITEM_ID)
        {
            return;
        }

        // Binding at an innkeeper hands out a replacement Hearthstone whenever the character is not
        // already carrying one -- which, once the stone has absorbed it, is always. Left alone, simply
        // rebinding your hearth would undo the bag slot the stone saved.
        //
        // Only if the AP stone is actually in hand: a character that has lost it still wants a hearthstone.
        if (!player->HasItemCount(AP_STONE_ITEM_ID, 1, true))
        {
            return;
        }

        // Safe here: Spell::DoCreateItem fires this hook as its last act on the item and never touches
        // it again.
        player->DestroyItemCount(HEARTHSTONE_ITEM_ID, 1, true);
        ChatHandler(player->GetSession()).SendSysMessage("Your Archipelago Stone absorbed the new Hearthstone.");
    }

    const char* AP_Stone::GetZoneTeleportIcon()
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

    const char* AP_Stone::GetDungeonTeleportIcon()
    {
        return player->GetTeamId(true) == TEAM_ALLIANCE
            ? "Icons/Achievement_Boss_EdwinVancleef"
            : "Icons/Spell_Shadow_SummonFelGuard";
    }

    void AP_Stone::SendMainMenu(Item* item)
    {
        StartGossipMenu(1, GOSSIP_MENU_MAIN);
        AddGossipItem("Icons/INV_Letter_03", "Open Mailbox", GOSSIP_ITEM_MAILBOX);

        // The real item shows its cooldown on the tooltip, which a gossip entry has not got, so the
        // label carries it instead. Location and cooldown never share the line: together they overflow
        // the gossip frame for the longer area names, Craftsmen's Terrace among them. Only one of the
        // two matters at a time anyway -- where the hearth sits while it can be used, how long is left
        // while it cannot.
        uint32 cooldown = player->GetSpellCooldownDelay(HEARTHSTONE_SPELL_ID);
        std::string hearthstone;

        if (cooldown > 0)
        {
            hearthstone = fmt::format("Hearthstone - ready in {}", secsToTimeString(CooldownSeconds(cooldown), true));
        }
        else
        {
            Optional<std::string> location = GetHearthstoneLocation();
            hearthstone = location.has_value() ? fmt::format("Hearthstone ({})", location.value()) : std::string("Hearthstone");
        }

        AddGossipItem("Icons/INV_Misc_Rune_01", hearthstone, GOSSIP_ITEM_HEARTHSTONE);

        if (HasAnyZoneUnlocked()) AddGossipItem(GetZoneTeleportIcon(), "Teleport to Zone", GOSSIP_ITEM_TELE_ZONE);
        if (HasAnyDungeonUnlocked()) AddGossipItem(GetDungeonTeleportIcon(), "Teleport to Dungeon", GOSSIP_ITEM_TELE_DUNGEON);
#ifdef MOD_ARCHIPELAWOW_TRANSMOG
        AddGossipItem("PaperDoll/UI-PaperDoll-Slot-Chest", "Transmogrification", GOSSIP_ITEM_TRANSMOG);
#endif
        SendGossipMenu(item);
    }

    void AP_Stone::HandleMainMenuAction(Item* item, uint32 action)
    {
        if (action == GOSSIP_ITEM_MAILBOX) HandleMailboxAction();
        else if (action == GOSSIP_ITEM_HEARTHSTONE) HandleHearthstoneAction();
        else if (action == GOSSIP_ITEM_TELE_ZONE) SendZoneTeleportMenu(item);
        else if (action == GOSSIP_ITEM_TELE_DUNGEON) SendDungeonTeleportMenu(item);
#ifdef MOD_ARCHIPELAWOW_TRANSMOG
        else if (action == GOSSIP_ITEM_TRANSMOG) HandleTransmogAction(item);
#endif
    }

    void AP_Stone::HandleMailboxAction()
    {
        player->PlayerTalkClass->SendCloseGossip();

        if (player->IsInCombat())
        {
            ChatHandler(player->GetSession()).SendSysMessage("|cFFFF0000Cannot do this while in combat.");
            return;
        }

        player->GetSession()->SendShowMailBox(player->GetGUID());
    }

    uint64 AP_Stone::CooldownSeconds(uint32 milliseconds)
    {
        // Rounded up, so a cooldown with anything left on it never reads as "0s".
        return (milliseconds + IN_MILLISECONDS - 1) / IN_MILLISECONDS;
    }

    void AP_Stone::HandleHearthstoneAction()
    {
        player->PlayerTalkClass->SendCloseGossip();

        // The client's "spell is not ready yet" carries no duration and there is no item tooltip to
        // read one off, so report it here. The cast below remains the authority; this only explains.
        uint32 cooldown = player->GetSpellCooldownDelay(HEARTHSTONE_SPELL_ID);
        if (cooldown > 0)
        {
            ChatHandler(player->GetSession()).SendSysMessage(
                fmt::format("|cFFFF0000Your Hearthstone is not ready for another {}.", secsToTimeString(CooldownSeconds(cooldown), true)));
            return;
        }

        // The client steps off a ground mount before using the real item, but the stone opens its menu
        // mounted, so the dismount falls to the server. A taxi or a flying mount in the air stays: the
        // cast then fails as the real item would.
        if (player->IsMounted() && !player->IsInFlight() && !player->IsFlying())
        {
            player->Dismount();
            player->RemoveAurasByType(SPELL_AURA_MOUNTED);
        }

        // Cast untriggered so the core runs the full CheckCast: the cooldown, the ten second cast and
        // its interrupts all apply exactly as they do to the real item. Nothing is lost by casting
        // without item 6948 in hand -- it carries spellcooldown_1 and spellcategorycooldown_1 of -1,
        // which Player::AddSpellAndCategoryCooldowns reads as "take the values from the spell".
        player->CastSpell(player, HEARTHSTONE_SPELL_ID, false);
    }

#ifdef MOD_ARCHIPELAWOW_TRANSMOG
    void AP_Stone::HandleTransmogAction(Item* item)
    {
        // mod-transmog's menu is a creature script, so it runs on a transmogrifier the stone summons for
        // the player, one nobody can see or click. CanSendGossipMessage makes its windows the stone's.
        Creature* transmogrifier = GetTransmogrifier();
        if (!transmogrifier)
        {
            player->PlayerTalkClass->SendCloseGossip();
            return;
        }

        transmogStoneGuid = item->GetGUID();
        transmogMenuShown = true;

        if (!sScriptMgr->OnGossipHello(player, transmogrifier))
        {
            player->PlayerTalkClass->SendCloseGossip();
        }
    }

    Creature* AP_Stone::GetTransmogrifier()
    {
        // Kept within reach of the player: mod-transmog's vendor interface opens a merchant window on
        // the transmogrifier itself, which the client closes out of range.
        Creature* transmogrifier = ObjectAccessor::GetCreature(*player, transmogrifierGuid);
        if (transmogrifier && transmogrifier->IsWithinDistInMap(player, INTERACTION_DISTANCE))
        {
            return transmogrifier;
        }

        if (transmogrifier)
        {
            transmogrifier->DespawnOrUnsummon();
        }

        TempSummon* summon = player->SummonCreature(TRANSMOGRIFIER_CREATURE_ID, player->GetPosition(), TEMPSUMMON_TIMED_DESPAWN, TRANSMOGRIFIER_LIFETIME, 0, nullptr, true);
        if (!summon)
        {
            return nullptr;
        }

        transmogrifierGuid = summon->GetGUID();
        summon->SetDisplayId(INVISIBLE_DISPLAY_ID);
        summon->SetUnitFlag(UNIT_FLAG_NOT_SELECTABLE);

        // With Transmogrification.EnablePortable, mod-transmog shows a summoned transmogrifier to its
        // owner only, and the client must know the creature before a merchant window opens on it.
        summon->SetOwnerGUID(player->GetGUID());
        player->UpdateVisibilityOf(summon);
        return summon;
    }
#endif

    Optional<std::string> AP_Stone::GetHearthstoneLocation()
    {
        // Where the character's hearth is bound, so the menu can name it the way the real item's
        // tooltip does. The client is told the same area id in SMSG_BINDPOINTUPDATE.
        const AreaTableEntry* area = sAreaTableStore.LookupEntry(player->m_homebindAreaId);
        if (!area)
        {
            return {};
        }

        // A DBC localized string can be absent for the session's locale, and naming nothing reads
        // better than naming an empty pair of brackets.
        const char* name = area->area_name[player->GetSession()->GetSessionDbcLocale()];
        if (!name || !*name)
        {
            return {};
        }

        return std::string(name);
    }

    void AP_Stone::SendZoneTeleportMenu(Item* item)
    {
        StartGossipMenu(1, GOSSIP_MENU_TELE_ZONE);
        if (HasAnyKalimdorZoneUnlocked()) AddGossipItem("Icons/Achievement_Zone_Kalimdor_01", "Kalimdor", GOSSIP_ITEM_TELE_ZONE_KALIMDOR);
        if (HasAnyEasternKingdomsZoneUnlocked()) AddGossipItem("Icons/Achievement_Zone_EasternKingdoms_01", "Eastern Kingdoms", GOSSIP_ITEM_TELE_ZONE_EASTERN_KINGDOMS);
        if (HasAnyOutlandZoneUnlocked()) AddGossipItem("Icons/Achievement_Zone_Outland_01", "Outland", GOSSIP_ITEM_TELE_ZONE_OUTLAND);
        if (HasAnyNorthrendZoneUnlocked()) AddGossipItem("Icons/Achievement_Zone_Northrend_01", "Northrend", GOSSIP_ITEM_TELE_ZONE_NORTHREND);
        AddGossipItemBack();
        SendGossipMenu(item);
    }

    void AP_Stone::HandleZoneTeleportSubmenuAction(Item* item, uint32 action)
    {
        if (action == GOSSIP_ITEM_TELE_ZONE_EASTERN_KINGDOMS) SendTeleportsGossipMenu(item, GOSSIP_MENU_TELE_ZONE_EASTERN_KINGDOMS);
        else if (action == GOSSIP_ITEM_TELE_ZONE_KALIMDOR) SendTeleportsGossipMenu(item, GOSSIP_MENU_TELE_ZONE_KALIMDOR);
        else if (action == GOSSIP_ITEM_TELE_ZONE_OUTLAND) SendTeleportsGossipMenu(item, GOSSIP_MENU_TELE_ZONE_OUTLAND);
        else if (action == GOSSIP_ITEM_TELE_ZONE_NORTHREND) SendTeleportsGossipMenu(item, GOSSIP_MENU_TELE_ZONE_NORTHREND);
        else if (action == GOSSIP_ITEM_BACK) SendMainMenu(item);
    }

    void AP_Stone::HandleZoneTeleportAction(Item* item, uint32 action)
    {
        if (action == GOSSIP_ITEM_BACK) SendZoneTeleportMenu(item);
        else HandleGenericTeleportAction(action);
    }

    void AP_Stone::SendDungeonTeleportMenu(Item* item)
    {
        StartGossipMenu(1, GOSSIP_MENU_TELE_DUNGEON);
        if (HasAnyClassicDungeonUnlocked()) AddGossipItem("Icons/Spell_Holy_ReviveChampion", "Classic", GOSSIP_ITEM_TELE_DUNGEON_CLASSIC);
        if (HasAnyOutlandDungeonUnlocked()) AddGossipItem("Icons/Spell_Holy_SummonChampion", "The Burning Crusade", GOSSIP_ITEM_TELE_DUNGEON_OUTLAND);
        if (HasAnyNorthrendDungeonUnlocked()) AddGossipItem("Icons/Spell_Holy_ChampionsBond", "Wrath of the Lich King", GOSSIP_ITEM_TELE_DUNGEON_NORTHREND);
        AddGossipItemBack();
        SendGossipMenu(item);
    }

    void AP_Stone::HandleDungeonTeleportSubmenuAction(Item* item, uint32 action)
    {
        if (action == GOSSIP_ITEM_TELE_DUNGEON_CLASSIC) SendTeleportsGossipMenu(item, GOSSIP_MENU_TELE_DUNGEON_CLASSIC);
        else if (action == GOSSIP_ITEM_TELE_DUNGEON_OUTLAND) SendTeleportsGossipMenu(item, GOSSIP_MENU_TELE_DUNGEON_OUTLAND);
        else if (action == GOSSIP_ITEM_TELE_DUNGEON_NORTHREND) SendTeleportsGossipMenu(item, GOSSIP_MENU_TELE_DUNGEON_NORTHREND);
        else if (action == GOSSIP_ITEM_BACK) SendMainMenu(item);
    }

    void AP_Stone::HandleDungeonTeleportAction(Item* item, uint32 action)
    {
        if (action == GOSSIP_ITEM_BACK) SendDungeonTeleportMenu(item);
        else HandleGenericTeleportAction(action);
    }

    void AP_Stone::HandleGenericTeleportAction(uint32 action)
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

    void AP_Stone::StartGossipMenu(uint32 titleTextId, uint32 sender)
    {
        player->PlayerTalkClass->ClearMenus();
#ifdef MOD_ARCHIPELAWOW_TRANSMOG
        transmogMenuShown = false;
#endif
        gossipIdx = 0;
        gossipSender = sender;
        gossipTitleTextId = titleTextId;
    }

    void AP_Stone::SendGossipMenu(Item* item)
    {
        player->PlayerTalkClass->SendGossipMenu(gossipTitleTextId, item->GetGUID());
    }

    void AP_Stone::AddGossipItem(const std::string& icon, const std::string& text, uint32 action)
    {
        player->PlayerTalkClass->GetGossipMenu().AddMenuItem(gossipIdx++, GOSSIP_ICON_CHAT, GossipItemText(icon, text), gossipSender, action, "", 0);
    }

    void AP_Stone::AddGossipItemBack()
    {
        AddGossipItem("PaperDollInfoFrame/UI-GearManager-Undo", "Back", GOSSIP_ITEM_BACK);
    }

    void AP_Stone::AddGossipItemBackToMainMenu()
    {
        AddGossipItem("PaperDollInfoFrame/UI-GearManager-LeaveItem-Opaque", "Back to main menu", GOSSIP_ITEM_BACK_TO_MAIN);
    }

    std::string AP_Stone::GossipItemText(const std::string& icon, const std::string& text)
    {
        return fmt::format("|TInterface/{}:32:32:12:-1|t    {}", icon, text);
    }

    void AP_Stone::SendTeleportsGossipMenu(Item* item, uint32 sender)
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

    bool AP_Stone::HasAnyEasternKingdomsZoneUnlocked()
    {
        return HasAnyZoneItemUnlocked(GOSSIP_MENU_TELE_ZONE_EASTERN_KINGDOMS);
    }

    bool AP_Stone::HasAnyKalimdorZoneUnlocked()
    {
        return HasAnyZoneItemUnlocked(GOSSIP_MENU_TELE_ZONE_KALIMDOR);
    }

    bool AP_Stone::HasAnyOutlandZoneUnlocked()
    {
        return HasAnyZoneItemUnlocked(GOSSIP_MENU_TELE_ZONE_OUTLAND);
    }

    bool AP_Stone::HasAnyNorthrendZoneUnlocked()
    {
        return HasAnyZoneItemUnlocked(GOSSIP_MENU_TELE_ZONE_NORTHREND);
    }

    bool AP_Stone::HasAnyZoneUnlocked()
    {
        return HasAnyEasternKingdomsZoneUnlocked() || HasAnyKalimdorZoneUnlocked() || HasAnyOutlandZoneUnlocked() || HasAnyNorthrendZoneUnlocked();
    }

    bool AP_Stone::HasAnyClassicDungeonUnlocked()
    {
        return HasAnyZoneItemUnlocked(GOSSIP_MENU_TELE_DUNGEON_CLASSIC);
    }

    bool AP_Stone::HasAnyOutlandDungeonUnlocked()
    {
        return HasAnyZoneItemUnlocked(GOSSIP_MENU_TELE_DUNGEON_OUTLAND);
    }

    bool AP_Stone::HasAnyNorthrendDungeonUnlocked()
    {
        return HasAnyZoneItemUnlocked(GOSSIP_MENU_TELE_DUNGEON_NORTHREND);
    }

    bool AP_Stone::HasAnyDungeonUnlocked()
    {
        return HasAnyClassicDungeonUnlocked() || HasAnyOutlandDungeonUnlocked() || HasAnyNorthrendDungeonUnlocked();
    }

    bool AP_Stone::HasAnyZoneItemUnlocked(uint32 menu)
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
}
