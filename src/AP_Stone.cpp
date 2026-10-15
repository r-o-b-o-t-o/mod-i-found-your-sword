#include "AP_Character.h"
#include "AP_Log.h"
#include "AP_Stone.h"
#include "ArchipelaWoW.h"
#include "Chat.h"
#include "Common.h"
#include "Config.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "Define.h"
#include "fmt/format.h"
#include "GameTime.h"
#include "GossipDef.h"
#include "Item.h"
#include "ItemPackets.h"
#include "items/AP_Zones.h"
#include "ItemTemplate.h"
#include "Map.h"
#include "Object.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Optional.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "Spell.h"
#include "SpellAuraDefines.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "Util.h"
#include "WorldPacket.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <functional>
#include <limits>
#include <mutex>
#include <string>
#include <string_view>
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
constexpr uint32 TRANSMOG_BACK_ITEM_ID = 100500; // archipelawow_world_010_transmog_back_item.sql
constexpr uint32 MAX_TRANSMOG_SUMMONS_PER_SECOND = 10;
#endif

constexpr uint32 GOSSIP_ITEM_GLYPHS = 6;
constexpr uint32 GOSSIP_ITEM_FRIEND_TELEPORT = 7;
constexpr uint32 GOSSIP_ITEM_PREFERENCES = 8;

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

constexpr uint32 GOSSIP_MENU_GLYPHS = 10;
constexpr uint32 GOSSIP_ITEM_GLYPHS_MAJOR = 1;
constexpr uint32 GOSSIP_ITEM_GLYPHS_MINOR = 2;

constexpr uint32 GOSSIP_MENU_GLYPHS_MAJOR = 11;
constexpr uint32 GOSSIP_MENU_GLYPHS_MINOR = 12;
constexpr uint32 GOSSIP_MENU_GLYPH_SLOTS = 13;

constexpr uint32 GOSSIP_MENU_FRIEND_TELEPORT = 14;

constexpr uint32 GOSSIP_MENU_PREFERENCES = 15;
constexpr uint32 GOSSIP_ITEM_PREFERENCES_HEIRLOOM_ROLE = 1;
constexpr uint32 GOSSIP_ITEM_PREFERENCES_HEIRLOOM_WEAPONS = 2;
constexpr uint32 GOSSIP_ITEM_PREFERENCES_ARMOR_TYPES = 3;
constexpr uint32 GOSSIP_ITEM_PREFERENCES_WORLD_BUFF_ROLE = 4;

constexpr uint32 GOSSIP_MENU_HEIRLOOM_WEAPON_SLOTS = 16;
constexpr uint32 GOSSIP_MENU_HEIRLOOM_WEAPONS = 17;

// archipelawow_world_011_glyph_texts.sql
constexpr uint32 GOSSIP_TEXT_GLYPHS = 100500;
constexpr uint32 GOSSIP_TEXT_GLYPH_SLOTS = 100501;

// archipelawow_world_012_stone_texts.sql
constexpr uint32 GOSSIP_TEXT_MAIN = 100502;
constexpr uint32 GOSSIP_TEXT_TELE_ZONE = 100503;
constexpr uint32 GOSSIP_TEXT_TELE_ZONE_LIST = 100504;
constexpr uint32 GOSSIP_TEXT_TELE_DUNGEON = 100505;
constexpr uint32 GOSSIP_TEXT_TELE_DUNGEON_LIST = 100506;

// archipelawow_world_013_friend_teleport.sql
constexpr uint32 GOSSIP_TEXT_FRIEND_TELEPORT = 100507;

// archipelawow_world_017_preference_texts.sql
constexpr uint32 GOSSIP_TEXT_PREFERENCES = 100508;
constexpr uint32 GOSSIP_TEXT_HEIRLOOM_WEAPON_SLOTS = 100509;
constexpr uint32 GOSSIP_TEXT_HEIRLOOM_WEAPONS = 100510;
// Serverside-only, it only carries the cooldown: the core saves a spell cooldown with the character.
constexpr uint32 FRIEND_TELEPORT_SPELL_ID = 100501;
constexpr uint32 FRIEND_TELEPORT_COOLDOWN = 30 * MINUTE * IN_MILLISECONDS;

// TypeFlags in GlyphProperties.dbc and GlyphSlot.dbc
constexpr uint32 GLYPH_TYPE_MAJOR = 0;
constexpr uint32 GLYPH_TYPE_MINOR = 1;

constexpr uint32 GOSSIP_ITEM_BACK = std::numeric_limits<uint32>::max() - 1;
constexpr uint32 GOSSIP_ITEM_BACK_TO_MAIN = std::numeric_limits<uint32>::max() - 2;

// Icon of the heaviest armor type the class can wear at max level
static const char* GetHeaviestArmorIcon(uint8 playerClass)
{
    switch (playerClass)
    {
        case CLASS_WARRIOR:
        case CLASS_PALADIN:
        case CLASS_DEATH_KNIGHT:
            return "Icons/INV_Chest_Plate03";
        case CLASS_HUNTER:
        case CLASS_SHAMAN:
            return "Icons/INV_Chest_Chain_06";
        case CLASS_DRUID:
        case CLASS_ROGUE:
            return "Icons/INV_Chest_Leather_07";
        default:
            return "Icons/INV_Chest_Cloth_30";
    }
}

namespace ModArchipelaWoW
{
    AP_Stone::AP_Stone(AP_Character* apCharacter) :
        apCharacter(apCharacter),
        player(apCharacter->GetPlayer()),
        gossipIdx(0),
        gossipSender(0),
        gossipTitleTextId(0),
        selectedGlyphItemId(0),
        selectedHeirloomWeaponSlot(0)
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
        // mod-transmog's senders overlap the stone's own (0 is its head slot), so only transmogStoneGuid
        // tells its menu apart.
        if (transmogStoneGuid)
        {
            if (!RefuseTransmogInCombat() && TakeTransmogSummon())
            {
                ForwardToTransmogrifier([&](Creature* transmogrifier)
                    {
                        return sScriptMgr->OnGossipSelect(player, transmogrifier, sender, action);
                    });
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
        else if (sender == GOSSIP_MENU_GLYPHS)
        {
            HandleGlyphsAction(item, action);
        }
        else if (sender == GOSSIP_MENU_GLYPHS_MAJOR || sender == GOSSIP_MENU_GLYPHS_MINOR)
        {
            HandleGlyphListAction(item, action);
        }
        else if (sender == GOSSIP_MENU_GLYPH_SLOTS)
        {
            HandleGlyphSlotAction(item, action);
        }
        else if (sender == GOSSIP_MENU_FRIEND_TELEPORT)
        {
            HandleFriendTeleportAction(item, action);
        }
        else if (sender == GOSSIP_MENU_PREFERENCES)
        {
            HandlePreferencesAction(item, action);
        }
        else if (sender == GOSSIP_MENU_HEIRLOOM_WEAPON_SLOTS)
        {
            HandleHeirloomWeaponSlotAction(item, action);
        }
        else if (sender == GOSSIP_MENU_HEIRLOOM_WEAPONS)
        {
            HandleHeirloomWeaponListAction(item, action);
        }
    }

#ifdef MOD_ARCHIPELAWOW_TRANSMOG
    void AP_Stone::OnGossipSelectCode(uint32 sender, uint32 action, const char* code)
    {
        // Only mod-transmog's menus ask for input: an item search or a set name.
        if (!transmogStoneGuid)
        {
            return;
        }

        if (RefuseTransmogInCombat() || !TakeTransmogSummon())
        {
            return;
        }

        ForwardToTransmogrifier([&](Creature* transmogrifier)
            {
                return sScriptMgr->OnGossipSelectCode(player, transmogrifier, sender, action, code);
            });
    }

    bool AP_Stone::CanPacketSend(const WorldPacket& packet)
    {
        if (!transmogrifierGuid)
        {
            // Any other merchant window replaces mod-transmog's. Not checked during a run: the copy sent
            // below comes back through this hook.
            if (packet.GetOpcode() == SMSG_LIST_INVENTORY)
            {
                transmogVendorShown = false;
            }

            return true;
        }

        if (packet.read<uint64>(0) != transmogrifierGuid.GetRawValue())
        {
            return true;
        }

        // mod-transmog opens its windows on the transmogrifier; they are moved off it before the client
        // sees them. Its menus go to the stone: they stay open wherever the player goes, and the core
        // hands their clicks to the stone with no range check. The merchant window of its vendor interface
        // goes to the player, as the stone's mailbox does: the client does not keep it open on the
        // transmogrifier, and CanPacketReceive hands its purchases back. The hook gets the packet
        // read-only, so it goes out again as a copy.
        ObjectGuid target = player->GetGUID();
        WorldPacket redirected(packet);
        if (packet.GetOpcode() == SMSG_GOSSIP_MESSAGE)
        {
            target = transmogStoneGuid;
            player->PlayerTalkClass->GetGossipMenu().SetSenderGUID(target);
        }
        else
        {
            AddTransmogBackItem(redirected);
            transmogVendorShown = true;
        }

        redirected.put<uint64>(0, target.GetRawValue());
        player->SendDirectMessage(&redirected);
        return false;
    }

    bool AP_Stone::CanPacketReceive(const WorldPacket& packet)
    {
        // Only purchases from mod-transmog's merchant window, while it is the last one sent and its menu is
        // still the stone's.
        if (!transmogVendorShown || !transmogStoneGuid || packet.read<uint64>(0) != player->GetGUID().GetRawValue())
        {
            return true;
        }

        uint32 item;
        uint32 slot;
        uint32 count;
        auto readBuy = [&](auto&& buy)
            {
                buy.Read();
                item = buy.Item;
                slot = buy.Slot;
                count = buy.Count;
            };

        if (packet.GetOpcode() == CMSG_BUY_ITEM)
        {
            readBuy(WorldPackets::Item::BuyItem{ WorldPacket(packet) });
        }
        else
        {
            readBuy(WorldPackets::Item::BuyItemInSlot{ WorldPacket(packet) });
        }

        // The client counts vendor slots from 1; the core's handler drops a 0.
        if (!slot || !IsTransmogEnabled())
        {
            return true;
        }

        if (RefuseTransmogInCombat() || !TakeTransmogSummon())
        {
            return false;
        }

        if (item == TRANSMOG_BACK_ITEM_ID)
        {
            OpenTransmogMenu();
            return false;
        }

        // A purchase in the merchant window CanPacketSend moved to the player, made again from a
        // transmogrifier for mod-transmog to take over. Where the item would go does not matter:
        // mod-transmog takes over every purchase from a transmogrifier and hands out no item.
        return !RunOnTransmogrifier([&](Creature* transmogrifier)
            {
                player->BuyItemFromVendorSlot(transmogrifier->GetGUID(), slot - 1, item, count, NULL_BAG, NULL_SLOT);
                return true;
            });
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

        // Not right away: destroying a new item deletes it, and the create-item hooks of the modules
        // loaded after this one (mod-transmog's among them) still read it. The player's next update
        // comes after all of them. The event belongs to the player, which outlives it; this AP_Stone
        // may not, so it is left out of the capture.
        player->m_Events.AddEventAtOffset([player = player]()
            {
                player->DestroyItemCount(HEARTHSTONE_ITEM_ID, 1, true);
                ChatHandler(player->GetSession()).SendSysMessage("Your Archipelago Stone absorbed the new Hearthstone.");
            }, 0ms);
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
        StartGossipMenu(GOSSIP_TEXT_MAIN, GOSSIP_MENU_MAIN);
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
            hearthstone = fmt::format("Hearthstone - |cFFC80000ready in {}", secsToTimeString(CooldownSeconds(cooldown), true));
        }
        else
        {
            Optional<std::string> location = GetHearthstoneLocation();
            hearthstone = location.has_value() ? fmt::format("Hearthstone ({})", location.value()) : std::string("Hearthstone");
        }

        AddGossipItem("Icons/INV_Misc_Rune_01", hearthstone, GOSSIP_ITEM_HEARTHSTONE);

        if (HasAnyZoneUnlocked()) AddGossipItem(GetZoneTeleportIcon(), "Teleport to Zone", GOSSIP_ITEM_TELE_ZONE);
        if (HasAnyDungeonUnlocked()) AddGossipItem(GetDungeonTeleportIcon(), "Teleport to Dungeon", GOSSIP_ITEM_TELE_DUNGEON);

        if (apCharacter->IsFriendTeleportUnlocked())
        {
            uint32 friendCooldown = player->GetSpellCooldownDelay(FRIEND_TELEPORT_SPELL_ID);
            std::string friendTeleport = friendCooldown > 0
                ? fmt::format("Teleport to Friend - |cFFC80000{}", secsToTimeString(CooldownSeconds(friendCooldown), true))
                : std::string("Teleport to Friend");
            AddGossipItem("Icons/Achievement_Reputation_01", friendTeleport, GOSSIP_ITEM_FRIEND_TELEPORT);
        }

        if (HasAnyGlyphAvailable()) AddGossipItem(GetGlyphIcon(GLYPH_TYPE_MAJOR), "Glyphs", GOSSIP_ITEM_GLYPHS);
#ifdef MOD_ARCHIPELAWOW_TRANSMOG
        if (IsTransmogEnabled()) AddGossipItem("Icons/INV_Fabric_Mageweave_02", "Transmogrification", GOSSIP_ITEM_TRANSMOG);
#endif
        if (HasAnyPreference())
        {
            std::string preferences = apCharacter->GetPendingHeirloomWeapons() > 0
                ? std::string("Preferences - |cFFC80000an heirloom weapon is waiting")
                : std::string("Preferences");
            AddGossipItem("Icons/INV_Misc_Gear_01", preferences, GOSSIP_ITEM_PREFERENCES);
        }
        SendGossipMenu(item);
    }

    void AP_Stone::HandleMainMenuAction(Item* item, uint32 action)
    {
        if (action == GOSSIP_ITEM_MAILBOX) HandleMailboxAction();
        else if (action == GOSSIP_ITEM_HEARTHSTONE) HandleHearthstoneAction();
        else if (action == GOSSIP_ITEM_TELE_ZONE) SendZoneTeleportMenu(item);
        else if (action == GOSSIP_ITEM_TELE_DUNGEON) SendDungeonTeleportMenu(item);
        else if (action == GOSSIP_ITEM_GLYPHS) SendGlyphsMenu(item);
        else if (action == GOSSIP_ITEM_FRIEND_TELEPORT) SendFriendTeleportMenu(item);
        else if (action == GOSSIP_ITEM_PREFERENCES) SendPreferencesMenu(item);
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

    void AP_Stone::DismountForItemCast()
    {
        // The client steps off a ground mount before using the real item, but the stone opens its menu
        // mounted, so the dismount falls to the server. A taxi or a flying mount in the air stays: the
        // cast then fails as the real item would.
        if (player->IsMounted() && !player->IsInFlight() && !player->IsFlying())
        {
            player->Dismount();
            player->RemoveAurasByType(SPELL_AURA_MOUNTED);
        }
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

        DismountForItemCast();

        // Cast untriggered so the core runs the full CheckCast: the cooldown, the ten second cast and
        // its interrupts all apply exactly as they do to the real item. Nothing is lost by casting
        // without item 6948 in hand -- it carries spellcooldown_1 and spellcategorycooldown_1 of -1,
        // which Player::AddSpellAndCategoryCooldowns reads as "take the values from the spell".
        player->CastSpell(player, HEARTHSTONE_SPELL_ID, false);
    }

#ifdef MOD_ARCHIPELAWOW_TRANSMOG
    void AP_Stone::HandleTransmogAction(Item* item)
    {
        // Before transmogStoneGuid is set: after a refusal the stone's own menu is still showing, and
        // its clicks must keep going to the stone rather than to mod-transmog.
        if (RefuseTransmogInCombat() || !TakeTransmogSummon())
        {
            return;
        }

        transmogStoneGuid = item->GetGUID();
        OpenTransmogMenu();
    }

    void AP_Stone::OpenTransmogMenu()
    {
        ForwardToTransmogrifier([&](Creature* transmogrifier)
            {
                return sScriptMgr->OnGossipHello(player, transmogrifier);
            });
    }

    void AP_Stone::AddTransmogBackItem(WorldPacket& list)
    {
        // The client does not tell the server when a merchant window closes, so the way back to
        // mod-transmog's menu is an entry in it, which CanPacketReceive answers. In a full list, it
        // takes the place of the last item.
        const ItemTemplate* back = sObjectMgr->GetItemTemplate(TRANSMOG_BACK_ITEM_ID);
        if (!back)
        {
            // Item templates only load at startup, so once is enough.
            static std::once_flag logged;
            std::call_once(logged, []()
                {
                    AP_LOG_ERROR("Item {} is missing, so mod-transmog's vendor interface has no Back entry. "
                        "Apply archipelawow_world_010_transmog_back_item.sql.", TRANSMOG_BACK_ITEM_ID);
                });
            return;
        }

        // Each item is 8 uint32, written as mod-transmog and the core write them: slot counted from
        // 1, item, display, stock (-1 for unlimited), price, durability, buy count, extended cost.
        uint8 count = list.read<uint8>(8);
        if (count == MAX_VENDOR_ITEMS)
        {
            list.wpos(list.wpos() - 8 * sizeof(uint32));
            --count;
        }

        list << uint32(count + 1) << back->ItemId << back->DisplayInfoID << int32(-1) << uint32(0) << back->MaxDurability << uint32(1) << uint32(0);
        list.put<uint8>(8, count + 1);
    }

    bool AP_Stone::IsTransmogEnabled()
    {
        // With this off, mod-transmog shows real gear in place of transmogs and hides the transmogrifiers
        // spawned in the world. Read straight from the config: this module never links against
        // mod-transmog's code, which may be built as a library of its own.
        return sConfigMgr->GetOption<bool>("Transmogrification.Enable", true, false);
    }

    bool AP_Stone::RefuseTransmogInCombat()
    {
        if (!player->IsInCombat())
        {
            return false;
        }

        // Like a throttled click, a refusal leaves the windows open, to click again out of combat.
        ChatHandler(player->GetSession()).SendSysMessage("|cFFFF0000Cannot do this while in combat.");
        return true;
    }

    bool AP_Stone::TakeTransmogSummon()
    {
        // Each click and purchase summons a transmogrifier, which uses up one of the map's creature
        // guids for as long as the map lives (a continent's, the whole uptime), and the server shuts
        // down once they run out. A client looping clicks or purchases could get there; the cap slows
        // one down to weeks. Clicking by hand rarely reaches it, short of lag delivering a burst of
        // clicks within the same second of world time, so a refusal leaves the windows as they are, to
        // click again.
        std::chrono::seconds now = GameTime::GetGameTime();
        if (now != transmogSummonSecond)
        {
            transmogSummonSecond = now;
            transmogSummons = 0;
        }

        if (++transmogSummons <= MAX_TRANSMOG_SUMMONS_PER_SECOND)
        {
            return true;
        }

        if (transmogSummons == MAX_TRANSMOG_SUMMONS_PER_SECOND + 1)
        {
            ChatHandler(player->GetSession()).SendSysMessage("|cFFFF0000You are doing that too fast.");
        }

        return false;
    }

    void AP_Stone::ForwardToTransmogrifier(const std::function<bool(Creature*)>& forward)
    {
        // As for RunOnTransmogrifier, callers take a summon with TakeTransmogSummon first.
        if (!IsTransmogEnabled() || !RunOnTransmogrifier(forward))
        {
            player->PlayerTalkClass->SendCloseGossip();
        }
    }

    bool AP_Stone::RunOnTransmogrifier(const std::function<bool(Creature*)>& run)
    {
        // mod-transmog's menu is a creature script, so each click and purchase runs on a transmogrifier
        // summoned for it alone. With no summoner, the summoner-only flag keeps it from every client;
        // CanPacketSend moves the windows it opens off it.
        //
        // Callers take a summon with TakeTransmogSummon first, before any state they would have to undo
        // on a refusal.
        TempSummon* transmogrifier = player->GetMap()->SummonCreature(TRANSMOGRIFIER_CREATURE_ID, player->GetPosition(), nullptr, 0, nullptr, 0, 0, true);
        if (!transmogrifier)
        {
            return false;
        }

        transmogrifierGuid = transmogrifier->GetGUID();
        bool done = run(transmogrifier);
        transmogrifierGuid.Clear();
        transmogrifier->DespawnOrUnsummon();
        return done;
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
        StartGossipMenu(GOSSIP_TEXT_TELE_ZONE, GOSSIP_MENU_TELE_ZONE);
        if (HasAnyKalimdorZoneUnlocked()) AddGossipItem("Icons/Achievement_Zone_Kalimdor_01", "Kalimdor", GOSSIP_ITEM_TELE_ZONE_KALIMDOR);
        if (HasAnyEasternKingdomsZoneUnlocked()) AddGossipItem("Icons/Achievement_Zone_EasternKingdoms_01", "Eastern Kingdoms", GOSSIP_ITEM_TELE_ZONE_EASTERN_KINGDOMS);
        if (HasAnyOutlandZoneUnlocked()) AddGossipItem("Icons/Achievement_Zone_Outland_01", "Outland", GOSSIP_ITEM_TELE_ZONE_OUTLAND);
        if (HasAnyNorthrendZoneUnlocked()) AddGossipItem("Icons/Achievement_Zone_Northrend_01", "Northrend", GOSSIP_ITEM_TELE_ZONE_NORTHREND);
        AddGossipItemBack();
        SendGossipMenu(item);
    }

    void AP_Stone::HandleZoneTeleportSubmenuAction(Item* item, uint32 action)
    {
        if (action == GOSSIP_ITEM_TELE_ZONE_EASTERN_KINGDOMS) SendTeleportsGossipMenu(item, GOSSIP_MENU_TELE_ZONE_EASTERN_KINGDOMS, GOSSIP_TEXT_TELE_ZONE_LIST);
        else if (action == GOSSIP_ITEM_TELE_ZONE_KALIMDOR) SendTeleportsGossipMenu(item, GOSSIP_MENU_TELE_ZONE_KALIMDOR, GOSSIP_TEXT_TELE_ZONE_LIST);
        else if (action == GOSSIP_ITEM_TELE_ZONE_OUTLAND) SendTeleportsGossipMenu(item, GOSSIP_MENU_TELE_ZONE_OUTLAND, GOSSIP_TEXT_TELE_ZONE_LIST);
        else if (action == GOSSIP_ITEM_TELE_ZONE_NORTHREND) SendTeleportsGossipMenu(item, GOSSIP_MENU_TELE_ZONE_NORTHREND, GOSSIP_TEXT_TELE_ZONE_LIST);
        else if (action == GOSSIP_ITEM_BACK) SendMainMenu(item);
    }

    void AP_Stone::HandleZoneTeleportAction(Item* item, uint32 action)
    {
        if (action == GOSSIP_ITEM_BACK) SendZoneTeleportMenu(item);
        else HandleGenericTeleportAction(action);
    }

    void AP_Stone::SendDungeonTeleportMenu(Item* item)
    {
        StartGossipMenu(GOSSIP_TEXT_TELE_DUNGEON, GOSSIP_MENU_TELE_DUNGEON);
        if (HasAnyClassicDungeonUnlocked()) AddGossipItem("Icons/Spell_Holy_ReviveChampion", "Classic", GOSSIP_ITEM_TELE_DUNGEON_CLASSIC);
        if (HasAnyOutlandDungeonUnlocked()) AddGossipItem("Icons/Spell_Holy_SummonChampion", "The Burning Crusade", GOSSIP_ITEM_TELE_DUNGEON_OUTLAND);
        if (HasAnyNorthrendDungeonUnlocked()) AddGossipItem("Icons/Spell_Holy_ChampionsBond", "Wrath of the Lich King", GOSSIP_ITEM_TELE_DUNGEON_NORTHREND);
        AddGossipItemBack();
        SendGossipMenu(item);
    }

    void AP_Stone::HandleDungeonTeleportSubmenuAction(Item* item, uint32 action)
    {
        if (action == GOSSIP_ITEM_TELE_DUNGEON_CLASSIC) SendTeleportsGossipMenu(item, GOSSIP_MENU_TELE_DUNGEON_CLASSIC, GOSSIP_TEXT_TELE_DUNGEON_LIST);
        else if (action == GOSSIP_ITEM_TELE_DUNGEON_OUTLAND) SendTeleportsGossipMenu(item, GOSSIP_MENU_TELE_DUNGEON_OUTLAND, GOSSIP_TEXT_TELE_DUNGEON_LIST);
        else if (action == GOSSIP_ITEM_TELE_DUNGEON_NORTHREND) SendTeleportsGossipMenu(item, GOSSIP_MENU_TELE_DUNGEON_NORTHREND, GOSSIP_TEXT_TELE_DUNGEON_LIST);
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

    void AP_Stone::SendFriendTeleportMenu(Item* item)
    {
        if (RefuseFriendTeleportOnCooldown())
        {
            return;
        }

        std::vector<Player*> friends = GetTeleportableFriends();
        if (friends.empty())
        {
            player->PlayerTalkClass->SendCloseGossip();
            ChatHandler(player->GetSession()).SendSysMessage("|cFFFF0000None of your friends are online and connected to the multiworld.");
            return;
        }

        StartGossipMenu(GOSSIP_TEXT_FRIEND_TELEPORT, GOSSIP_MENU_FRIEND_TELEPORT);
        for (Player* target : friends)
        {
            // The last entry is for Back, as in SendGlyphListMenu.
            if (gossipIdx == GOSSIP_MAX_MENU_ITEMS - 1)
            {
                break;
            }

            AddGossipItem(GetCharacterIcon(target), target->GetName(), target->GetGUID().GetCounter());
        }
        AddGossipItemBack();
        SendGossipMenu(item);
    }

    void AP_Stone::HandleFriendTeleportAction(Item* item, uint32 action)
    {
        if (action == GOSSIP_ITEM_BACK)
        {
            SendMainMenu(item);
            return;
        }

        // Checked again rather than trusted from when the menu was sent: it can stay open for a while.
        if (RefuseFriendTeleportOnCooldown())
        {
            return;
        }

        player->PlayerTalkClass->SendCloseGossip();
        ChatHandler chat(player->GetSession());

        std::vector<Player*> friends = GetTeleportableFriends();
        auto target = std::ranges::find_if(friends, [&](Player* candidate)
            {
                return candidate->GetGUID().GetCounter() == action;
            });

        if (target == friends.end())
        {
            chat.SendSysMessage("|cFFFF0000Your friend is no longer online and connected to the multiworld.");
            return;
        }

        Player* destination = *target;
        if (player->IsInCombat())
        {
            chat.SendSysMessage("|cFFFF0000Cannot do this while in combat.");
            return;
        }

        // In a dungeon, only the friend's group is sure to share their instance. A battleground or an arena
        // is never joined this way.
        Map* map = destination->GetMap();
        if (map->IsBattlegroundOrArena())
        {
            chat.SendSysMessage(fmt::format("|cFFFF0000{} is in a battleground.", destination->GetName()));
            return;
        }

        if (map->IsDungeon() && (!player->GetGroup() || player->GetGroup() != destination->GetGroup()))
        {
            chat.SendSysMessage(fmt::format("|cFFFF0000{} is in a dungeon, join their group to teleport to them.", destination->GetName()));
            return;
        }

        // On a taxi or a boat the position means nothing, and in the air on a flying mount the character
        // would arrive with nothing to stand on.
        if (destination->IsInFlight() || destination->GetTransport() || destination->IsFlying())
        {
            chat.SendSysMessage(fmt::format("|cFFFF0000{} is travelling, try again once they have arrived.", destination->GetName()));
            return;
        }

        // The character would only be sent back on arrival.
        if (apCharacter->IsZoneLocked(destination->GetZoneId()))
        {
            chat.SendSysMessage(fmt::format("|cFFFF0000{} is in a zone you have not unlocked.", destination->GetName()));
            return;
        }

        if (apCharacter->TeleportToPlayer(destination))
        {
            player->AddSpellCooldown(FRIEND_TELEPORT_SPELL_ID, 0, FRIEND_TELEPORT_COOLDOWN);
        }
    }

    bool AP_Stone::RefuseFriendTeleportOnCooldown()
    {
        uint32 cooldown = player->GetSpellCooldownDelay(FRIEND_TELEPORT_SPELL_ID);
        if (cooldown == 0)
        {
            return false;
        }

        player->PlayerTalkClass->SendCloseGossip();
        ChatHandler(player->GetSession()).SendSysMessage(
            fmt::format("|cFFFF0000You cannot teleport to a friend for another {}.", secsToTimeString(CooldownSeconds(cooldown), true)));
        return true;
    }

    std::vector<Player*> AP_Stone::GetTeleportableFriends()
    {
        std::vector<Player*> friends;
        for (const auto& [guid, character] : sArchipelaWoW->GetCharacters())
        {
            Player* candidate = character->GetPlayer();
            if (character != apCharacter && character->IsSlotConnected() && candidate && candidate->IsInWorld())
            {
                friends.push_back(candidate);
            }
        }

        std::ranges::sort(friends, [](Player* a, Player* b)
            {
                return a->GetName() < b->GetName();
            });

        return friends;
    }

    std::string AP_Stone::GetCharacterIcon(Player* target)
    {
        const char* race;
        switch (target->getRace(true))
        {
            case RACE_HUMAN: race = "Human"; break;
            case RACE_ORC: race = "Orc"; break;
            case RACE_DWARF: race = "Dwarf"; break;
            case RACE_NIGHTELF: race = "Nightelf"; break;
            case RACE_UNDEAD_PLAYER: race = "Undead"; break;
            case RACE_TAUREN: race = "Tauren"; break;
            case RACE_GNOME: race = "Gnome"; break;
            case RACE_TROLL: race = "Troll"; break;
            case RACE_BLOODELF: race = "Bloodelf"; break;
            case RACE_DRAENEI: race = "Draenei"; break;
            default: return "Icons/INV_Misc_QuestionMark";
        }

        return fmt::format("Icons/Achievement_Character_{}_{}", race, target->getGender() == GENDER_FEMALE ? "Female" : "Male");
    }

    bool AP_Stone::HasAnyGlyphAvailable()
    {
        // No glyph slot unlocks before level 15.
        return player->GetUInt32Value(PLAYER_GLYPHS_ENABLED) != 0
            && std::ranges::any_of(apCharacter->GetReceivedGlyphs(), [&](uint32 itemId)
                {
                    return GetAvailableGlyphSpell(itemId).has_value();
                });
    }

    bool AP_Stone::HasAnyGlyphAvailable(uint32 type)
    {
        return std::ranges::any_of(apCharacter->GetReceivedGlyphs(), [&](uint32 itemId)
            {
                Optional<GlyphSpell> glyphSpell = GetAvailableGlyphSpell(itemId);
                return glyphSpell && glyphSpell->glyph->TypeFlags == type;
            });
    }

    void AP_Stone::SendGlyphsMenu(Item* item)
    {
        StartGossipMenu(GOSSIP_TEXT_GLYPHS, GOSSIP_MENU_GLYPHS);
        if (HasAnyGlyphAvailable(GLYPH_TYPE_MAJOR)) AddGossipItem(GetGlyphIcon(GLYPH_TYPE_MAJOR), "Major Glyphs", GOSSIP_ITEM_GLYPHS_MAJOR);
        if (HasAnyGlyphAvailable(GLYPH_TYPE_MINOR)) AddGossipItem(GetGlyphIcon(GLYPH_TYPE_MINOR), "Minor Glyphs", GOSSIP_ITEM_GLYPHS_MINOR);
        AddGossipItemBack();
        SendGossipMenu(item);
    }

    void AP_Stone::HandleGlyphsAction(Item* item, uint32 action)
    {
        if (action == GOSSIP_ITEM_GLYPHS_MAJOR) SendGlyphListMenu(item, GLYPH_TYPE_MAJOR);
        else if (action == GOSSIP_ITEM_GLYPHS_MINOR) SendGlyphListMenu(item, GLYPH_TYPE_MINOR);
        else if (action == GOSSIP_ITEM_BACK) SendMainMenu(item);
    }

    void AP_Stone::SendGlyphListMenu(Item* item, uint32 type)
    {
        StartGossipMenu(GOSSIP_TEXT_GLYPHS, type == GLYPH_TYPE_MINOR ? GOSSIP_MENU_GLYPHS_MINOR : GOSSIP_MENU_GLYPHS_MAJOR);
        for (const auto& glyph : GetAvailableGlyphs(type))
        {
            // The client shows no more than GOSSIP_MAX_MENU_ITEMS entries (and the core asserts on the
            // second one past it); the last one is for Back.
            if (gossipIdx == GOSSIP_MAX_MENU_ITEMS - 1)
            {
                break;
            }

            AddGossipItem(glyph.icon, glyph.name, glyph.itemId);
        }
        AddGossipItemBack();
        SendGossipMenu(item);
    }

    void AP_Stone::HandleGlyphListAction(Item* item, uint32 action)
    {
        if (action == GOSSIP_ITEM_BACK)
        {
            SendGlyphsMenu(item);
        }
        else if (apCharacter->GetReceivedGlyphs().contains(action))
        {
            selectedGlyphItemId = action;
            SendGlyphSlotsMenu(item);
        }
    }

    void AP_Stone::SendGlyphSlotsMenu(Item* item)
    {
        Optional<GlyphSpell> glyphSpell = GetGlyphSpell(selectedGlyphItemId);
        if (!glyphSpell)
        {
            SendGlyphsMenu(item);
            return;
        }

        std::string icon = GetItemIcon(sObjectMgr->GetItemTemplate(selectedGlyphItemId));

        // The slots of the glyph's type, in slot order. Those the character's level has not unlocked yet
        // are left out.
        StartGossipMenu(GOSSIP_TEXT_GLYPH_SLOTS, GOSSIP_MENU_GLYPH_SLOTS);
        uint32 enabledSlots = player->GetUInt32Value(PLAYER_GLYPHS_ENABLED);
        for (uint8 slot = 0; slot < MAX_GLYPH_SLOT_INDEX; ++slot)
        {
            const GlyphSlotEntry* slotEntry = sGlyphSlotStore.LookupEntry(player->GetGlyphSlot(slot));
            if (!slotEntry || slotEntry->TypeFlags != glyphSpell->glyph->TypeFlags || !(enabledSlots & (1 << slot)))
            {
                continue;
            }

            const GlyphPropertiesEntry* current = sGlyphPropertiesStore.LookupEntry(player->GetGlyph(slot));
            std::string text = current ? fmt::format("|cFFC80000Replace |r{}", GetInscribedGlyphName(current)) : std::string("Empty slot");
            AddGossipItem(icon, text, slot);
        }
        AddGossipItemBack();
        AddGossipItemBackToMainMenu();
        SendGossipMenu(item);
    }

    void AP_Stone::HandleGlyphSlotAction(Item* item, uint32 action)
    {
        Optional<GlyphSpell> glyphSpell = GetGlyphSpell(selectedGlyphItemId);
        if (!glyphSpell)
        {
            player->PlayerTalkClass->SendCloseGossip();
            return;
        }

        if (action == GOSSIP_ITEM_BACK)
        {
            SendGlyphListMenu(item, glyphSpell->glyph->TypeFlags);
            return;
        }

        player->PlayerTalkClass->SendCloseGossip();

        if (player->IsInCombat())
        {
            ChatHandler(player->GetSession()).SendSysMessage("|cFFFF0000Cannot do this while in combat.");
            return;
        }

        // No glyph spell can be cast mounted.
        DismountForItemCast();

        // Cast as Player::CastItemUseSpell casts a glyph item the client used on a slot, short of the
        // item: a plain cast leaves the slot index at 0. Untriggered, so the core still runs its own
        // checks on the glyph and the slot.
        Spell* spell = new Spell(player, glyphSpell->spell, TRIGGERED_NONE);
        spell->m_glyphIndex = action;
        SpellCastTargets targets;
        targets.SetUnitTarget(player);
        spell->prepare(&targets);
    }

    Optional<AP_Stone::GlyphSpell> AP_Stone::GetGlyphSpell(uint32 itemId)
    {
        const ItemTemplate* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        if (!itemTemplate)
        {
            return {};
        }

        for (const auto& itemSpell : itemTemplate->Spells)
        {
            const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(itemSpell.SpellId);
            if (!spellInfo)
            {
                continue;
            }

            for (const auto& effect : spellInfo->Effects)
            {
                if (effect.Effect != SPELL_EFFECT_APPLY_GLYPH)
                {
                    continue;
                }

                if (const GlyphPropertiesEntry* glyph = sGlyphPropertiesStore.LookupEntry(effect.MiscValue))
                {
                    return GlyphSpell{ spellInfo, glyph };
                }
            }
        }

        return {};
    }

    Optional<AP_Stone::GlyphSpell> AP_Stone::GetAvailableGlyphSpell(uint32 itemId)
    {
        const ItemTemplate* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        Optional<GlyphSpell> glyphSpell = GetGlyphSpell(itemId);
        if (!itemTemplate || !glyphSpell)
        {
            return {};
        }

        // Left out until the character reaches the item's level, as the item itself would refuse,
        // and while inscribed: the core refuses a glyph whose aura the character already has.
        if (player->CanUseItem(itemTemplate) != EQUIP_ERR_OK || player->HasAura(glyphSpell->glyph->SpellId))
        {
            return {};
        }

        return glyphSpell;
    }

    std::vector<AP_Stone::AvailableGlyph> AP_Stone::GetAvailableGlyphs(uint32 type)
    {
        std::vector<AvailableGlyph> glyphs;
        for (uint32 itemId : apCharacter->GetReceivedGlyphs())
        {
            Optional<GlyphSpell> glyphSpell = GetAvailableGlyphSpell(itemId);
            if (glyphSpell && glyphSpell->glyph->TypeFlags == type)
            {
                glyphs.push_back({ itemId, GetGlyphItemName(itemId), GetItemIcon(sObjectMgr->GetItemTemplate(itemId)) });
            }
        }

        std::ranges::sort(glyphs, [](const AvailableGlyph& a, const AvailableGlyph& b)
            {
                return a.name < b.name;
            });

        return glyphs;
    }

    std::string AP_Stone::GetGlyphItemName(uint32 itemId)
    {
        return StripGlyphPrefix(GetItemName(itemId));
    }

    std::string AP_Stone::GetInscribedGlyphName(const GlyphPropertiesEntry* glyph)
    {
        for (uint32 itemId : apCharacter->GetReceivedGlyphs())
        {
            Optional<GlyphSpell> glyphSpell = GetGlyphSpell(itemId);
            if (glyphSpell && glyphSpell->glyph->Id == glyph->Id)
            {
                return GetGlyphItemName(itemId);
            }
        }

        // A glyph the slot never received has no item to name it. Its aura's name is the item's but
        // for a handful, such as "Glyph of Growling" for the Glyph of Growl.
        const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(glyph->SpellId);
        const char* name = spellInfo ? spellInfo->SpellName[player->GetSession()->GetSessionDbcLocale()] : nullptr;
        return StripGlyphPrefix(name ? name : "");
    }

    std::string AP_Stone::StripGlyphPrefix(std::string name)
    {
        // Every entry is a glyph, and the prefix makes "Replace Glyph of Righteous Defense" overflow the
        // gossip frame. Only the English one is known.
        constexpr std::string_view prefix = "Glyph of ";
        if (name.starts_with(prefix) && name.size() > prefix.size())
        {
            name.erase(0, prefix.size());
            name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
        }

        return name;
    }

    std::string AP_Stone::GetGlyphIcon(uint32 type)
    {
        const char* kind = type == GLYPH_TYPE_MINOR ? "Minor" : "Major";
        switch (player->getClass())
        {
            case CLASS_WARRIOR: return fmt::format("Icons/INV_Glyph_{}Warrior", kind);
            case CLASS_PALADIN: return fmt::format("Icons/INV_Glyph_{}Paladin", kind);
            case CLASS_HUNTER: return fmt::format("Icons/INV_Glyph_{}Hunter", kind);
            case CLASS_ROGUE: return fmt::format("Icons/INV_Glyph_{}Rogue", kind);
            case CLASS_PRIEST: return fmt::format("Icons/INV_Glyph_{}Priest", kind);
            case CLASS_DEATH_KNIGHT: return fmt::format("Icons/INV_Glyph_{}DeathKnight", kind);
            case CLASS_SHAMAN: return fmt::format("Icons/INV_Glyph_{}Shaman", kind);
            case CLASS_MAGE: return fmt::format("Icons/INV_Glyph_{}Mage", kind);
            case CLASS_WARLOCK: return fmt::format("Icons/INV_Glyph_{}Warlock", kind);
            default: return fmt::format("Icons/INV_Glyph_{}Druid", kind);
        }
    }

    bool AP_Stone::HasAnyPreference()
    {
        return apCharacter->IsHybrid() || HasHeirloomWeaponChoice();
    }

    bool AP_Stone::HasHeirloomWeaponChoice()
    {
        // With a single weapon to choose from, the slot fills itself.
        const Items::Heirlooms& heirlooms = apCharacter->GetItemsContainer().heirlooms;
        return heirlooms.weaponSlots > 0 && heirlooms.weaponChoices.size() > 1;
    }

    void AP_Stone::SendPreferencesMenu(Item* item)
    {
        StartGossipMenu(GOSSIP_TEXT_PREFERENCES, GOSSIP_MENU_PREFERENCES);

        if (apCharacter->IsHybrid())
        {
            bool caster = apCharacter->PrefersCasterHeirlooms();
            AddGossipItem(caster ? "Icons/Spell_Arcane_FocusedPower" : "Icons/Spell_Nature_Strength",
                fmt::format("Heirloom armor: {}", caster ? "Caster" : "Melee"), GOSSIP_ITEM_PREFERENCES_HEIRLOOM_ROLE);
        }

        if (HasHeirloomWeaponChoice())
        {
            uint32 icon = apCharacter->GetHeirloomWeapon(0);
            if (!icon)
            {
                icon = apCharacter->GetItemsContainer().heirlooms.weaponChoices.front();
            }

            std::string weapons = apCharacter->GetPendingHeirloomWeapons() > 0
                ? std::string("Heirloom weapons - |cFFC80000one is waiting")
                : std::string("Heirloom weapons");
            AddGossipItem(GetItemIcon(icon), weapons, GOSSIP_ITEM_PREFERENCES_HEIRLOOM_WEAPONS);
        }

        if (apCharacter->IsHybrid())
        {
            bool allTypes = apCharacter->PrefersAllArmorTypes();
            AddGossipItem(allTypes ? GetHeaviestArmorIcon(CLASS_MAGE) : GetHeaviestArmorIcon(player->getClass()),
                fmt::format("Random armor: {}", allTypes ? "All types" : "Heaviest type only"), GOSSIP_ITEM_PREFERENCES_ARMOR_TYPES);
        }

        // Not for seeds generated before the apworld had world buffs
        if (apCharacter->IsHybrid() && apCharacter->GetItemsContainer().worldBuffs.apItemId != 0)
        {
            // Slip'kik's Savvy and Fengus' Ferocity
            bool caster = apCharacter->PrefersCasterWorldBuffs();
            AddGossipItem(caster ? "Icons/Spell_Holy_LesserHeal02" : "Icons/Spell_Nature_UndyingStrength",
                fmt::format("World buffs: {}", caster ? "Caster" : "Melee"), GOSSIP_ITEM_PREFERENCES_WORLD_BUFF_ROLE);
        }

        AddGossipItemBack();
        SendGossipMenu(item);
    }

    void AP_Stone::HandlePreferencesAction(Item* item, uint32 action)
    {
        if (action == GOSSIP_ITEM_PREFERENCES_HEIRLOOM_ROLE)
        {
            apCharacter->ToggleCasterHeirlooms();
            SendPreferencesMenu(item);
        }
        else if (action == GOSSIP_ITEM_PREFERENCES_HEIRLOOM_WEAPONS)
        {
            SendHeirloomWeaponSlotsMenu(item);
        }
        else if (action == GOSSIP_ITEM_PREFERENCES_ARMOR_TYPES)
        {
            apCharacter->ToggleAllArmorTypes();
            SendPreferencesMenu(item);
        }
        else if (action == GOSSIP_ITEM_PREFERENCES_WORLD_BUFF_ROLE)
        {
            apCharacter->ToggleCasterWorldBuffs();
            SendPreferencesMenu(item);
        }
        else if (action == GOSSIP_ITEM_BACK)
        {
            SendMainMenu(item);
        }
    }

    void AP_Stone::SendHeirloomWeaponSlotsMenu(Item* item)
    {
        StartGossipMenu(GOSSIP_TEXT_HEIRLOOM_WEAPON_SLOTS, GOSSIP_MENU_HEIRLOOM_WEAPON_SLOTS);

        // Sent in slot order, so the weapons that are waiting belong to the first slots not sent yet.
        uint8 slots = apCharacter->GetItemsContainer().heirlooms.weaponSlots;
        uint32 waiting = apCharacter->GetPendingHeirloomWeapons();
        for (uint8 slot = 0; slot < slots; ++slot)
        {
            uint32 weapon = apCharacter->GetHeirloomWeapon(slot);
            if (apCharacter->IsHeirloomWeaponSent(slot))
            {
                AddGossipItem(GetItemIcon(weapon), fmt::format("|cFF646464{}", GetItemName(weapon)), slot);
                continue;
            }

            bool arrived = waiting > 0;
            if (arrived)
            {
                --waiting;
            }

            if (weapon)
            {
                AddGossipItem(GetItemIcon(weapon), GetItemName(weapon), slot);
            }
            else
            {
                AddGossipItem("Icons/INV_Misc_QuestionMark", arrived ? "Empty slot - |cFFC80000its weapon is waiting" : "Empty slot", slot);
            }
        }

        AddGossipItemBack();
        AddGossipItemBackToMainMenu();
        SendGossipMenu(item);
    }

    void AP_Stone::HandleHeirloomWeaponSlotAction(Item* item, uint32 action)
    {
        if (action == GOSSIP_ITEM_BACK)
        {
            SendPreferencesMenu(item);
        }
        else if (action < apCharacter->GetItemsContainer().heirlooms.weaponSlots && !apCharacter->IsHeirloomWeaponSent(action))
        {
            selectedHeirloomWeaponSlot = static_cast<uint8>(action);
            SendHeirloomWeaponListMenu(item);
        }
        else
        {
            // A weapon already sent stays as it is.
            SendHeirloomWeaponSlotsMenu(item);
        }
    }

    void AP_Stone::SendHeirloomWeaponListMenu(Item* item)
    {
        StartGossipMenu(GOSSIP_TEXT_HEIRLOOM_WEAPONS, GOSSIP_MENU_HEIRLOOM_WEAPONS);
        for (uint32 weapon : apCharacter->GetItemsContainer().heirlooms.weaponChoices)
        {
            AddGossipItem(GetItemIcon(weapon), GetItemName(weapon), weapon);
        }
        AddGossipItemBack();
        AddGossipItemBackToMainMenu();
        SendGossipMenu(item);
    }

    void AP_Stone::HandleHeirloomWeaponListAction(Item* item, uint32 action)
    {
        if (action != GOSSIP_ITEM_BACK)
        {
            apCharacter->SetHeirloomWeapon(selectedHeirloomWeaponSlot, action);
        }

        SendHeirloomWeaponSlotsMenu(item);
    }

    std::string AP_Stone::GetItemName(uint32 itemId)
    {
        const ItemTemplate* itemTemplate = sObjectMgr->GetItemTemplate(itemId);
        if (!itemTemplate)
        {
            return "";
        }

        std::string name = itemTemplate->Name1;
        if (const ItemLocale* itemLocale = sObjectMgr->GetItemLocale(itemId))
        {
            ObjectMgr::GetLocaleString(itemLocale->Name, player->GetSession()->GetSessionDbLocaleIndex(), name);
        }

        return name;
    }

    std::string AP_Stone::GetItemIcon(uint32 itemId)
    {
        return GetItemIcon(sObjectMgr->GetItemTemplate(itemId));
    }

    std::string AP_Stone::GetItemIcon(const ItemTemplate* itemTemplate)
    {
        const ItemDisplayInfoEntry* display = itemTemplate ? sItemDisplayInfoStore.LookupEntry(itemTemplate->DisplayInfoID) : nullptr;
        return display ? fmt::format("Icons/{}", display->inventoryIcon) : "Icons/INV_Misc_QuestionMark";
    }

    void AP_Stone::StartGossipMenu(uint32 titleTextId, uint32 sender)
    {
        player->PlayerTalkClass->ClearMenus();
#ifdef MOD_ARCHIPELAWOW_TRANSMOG
        transmogStoneGuid.Clear();
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
        AddGossipItem("PaperDollInfoFrame/UI-GearManager-Undo", "|cFF323232Back", GOSSIP_ITEM_BACK);
    }

    void AP_Stone::AddGossipItemBackToMainMenu()
    {
        AddGossipItem("PaperDollInfoFrame/UI-GearManager-LeaveItem-Opaque", "|cFF323232Back to main menu", GOSSIP_ITEM_BACK_TO_MAIN);
    }

    std::string AP_Stone::GossipItemText(const std::string& icon, const std::string& text)
    {
        return fmt::format("|TInterface/{}:32:32:12:-1|t    {}", icon, text);
    }

    void AP_Stone::SendTeleportsGossipMenu(Item* item, uint32 sender, uint32 titleTextId)
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

        StartGossipMenu(titleTextId, sender);
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
