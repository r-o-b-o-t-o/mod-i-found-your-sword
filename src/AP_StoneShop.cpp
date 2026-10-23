#include "AP_Character.h"
#include "AP_Stone.h"
#include "AP_StoneShop.h"
#include "Chat.h"
#include "Creature.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "Define.h"
#include "fmt/format.h"
#include "ItemPackets.h"
#include "ItemTemplate.h"
#include "locations/AP_Shop.h"
#include "network/AP_Client.h"
#include "ObjectGuid.h"
#include "Opcodes.h"
#include "Player.h"
#include "SharedDefines.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <list>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// The item entries the shop lists its items under, one per item of the shop, in its order. They have
// no item_template row: the shop answers the client's queries for them with what each item holds.
constexpr uint32 SHOP_ITEM_ENTRY = 100600;

struct ShopPrice
{
    uint8 level;
    uint32 price;
};

// The price of each of a tier's items when the tier sells three, by the level the tier goes on sale at.
// About 0.7 levels' worth of the copper, quest money and vendored loot a character earns at that level,
// as simulated with tools/gold_sim in the apworld repository. Tiers 25 and 30 sit a little under the
// curve: the class trainers take the most out of those brackets.
constexpr ShopPrice SHOP_PRICES[] = {
    { 1, 1 * SILVER + 40 * COPPER },
    { 5, 7 * SILVER },
    { 10, 35 * SILVER },
    { 15, 60 * SILVER },
    { 20, 1 * GOLD },
    { 25, 1 * GOLD + 50 * SILVER },
    { 30, 2 * GOLD + 40 * SILVER },
    { 35, 4 * GOLD + 80 * SILVER },
    { 40, 8 * GOLD },
    { 45, 14 * GOLD },
    { 50, 25 * GOLD },
    { 55, 45 * GOLD },
    { 60, 75 * GOLD },
    { 65, 140 * GOLD },
    { 70, 240 * GOLD },
    { 75, 420 * GOLD },
};

// The tier price above is for this many items per tier: a tier with more sells each for less.
constexpr uint32 SHOP_PRICED_ITEMS_PER_TIER = 3;

using NetworkItem = ModArchipelaWoW::Network::Client::NetworkItem;
using Client = ModArchipelaWoW::Network::Client;

// The gifts are other players' items, the rest the character's own. Where a kind has several icons,
// the location picks one.
static const char* GetIcon(const NetworkItem& item, bool own)
{
    static const char* const crates[] = { "INV_Crate_01", "INV_Crate_02", "INV_Crate_03", "INV_Crate_04", "INV_Crate_05", "INV_Crate_06" };
    static const char* const bombs[] = { "INV_Misc_Bomb_01", "INV_Misc_Bomb_03", "INV_Misc_Bomb_05", "INV_Misc_Bomb_07", "INV_Misc_Bomb_09" };

    if (item.flags & Client::FlagAdvancement) return own ? "INV_Jewelcrafting_DragonsEye02" : "INV_Misc_Gift_02";
    if (item.flags & Client::FlagNeverExclude) return own ? "INV_Ore_Arcanite_01" : "INV_Misc_Gift_03";
    if (item.flags & Client::FlagTrap) return own ? bombs[item.location % std::size(bombs)] : "INV_Misc_Gift_04";
    return own ? crates[item.location % std::size(crates)] : "INV_Misc_Gift_05";
}

static std::string ToLower(std::string text)
{
    std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

static uint32 GetQuality(const NetworkItem& item)
{
    if (item.flags & Client::FlagAdvancement) return ITEM_QUALITY_EPIC;
    if (item.flags & Client::FlagNeverExclude) return ITEM_QUALITY_RARE;
    if (item.flags & Client::FlagTrap) return ITEM_QUALITY_POOR;
    return ITEM_QUALITY_NORMAL;
}

static const char* GetKind(const NetworkItem& item)
{
    if (item.flags & Client::FlagAdvancement) return "Progression";
    if (item.flags & Client::FlagNeverExclude) return "Useful";
    if (item.flags & Client::FlagTrap) return "Trap";
    return "Filler";
}

namespace ModArchipelaWoW
{
    AP_StoneShop::AP_StoneShop(AP_Character* apCharacter, AP_Stone* stone) :
        apCharacter(apCharacter),
        stone(stone),
        player(apCharacter->GetPlayer()),
        scoutedItems()
    {
    }

    bool AP_StoneShop::HasItemsLeft() const
    {
        return std::ranges::any_of(apCharacter->GetLocationsContainer().shop.GetItems(), [&](const Locations::ShopItem& item)
            {
                return !apCharacter->IsLocationChecked(item.locationId);
            });
    }

    void AP_StoneShop::Open()
    {
        // Opened on the player, as the stone's mailbox is: the client keeps the window open wherever the
        // player goes, and sends the player as the vendor of its purchases.
        WorldPacket list(SMSG_LIST_INVENTORY, 8 + 1 + MAX_VENDOR_ITEMS * 8 * 4);
        list << player->GetGUID();
        list << uint8(0);

        const std::vector<Locations::ShopItem>& items = apCharacter->GetLocationsContainer().shop.GetItems();
        std::vector<int64> shown;
        uint8 count = 0;
        for (size_t i = 0; i < items.size() && i < MAX_VENDOR_ITEMS; ++i)
        {
            const Locations::ShopItem& item = items[i];
            if (apCharacter->IsLocationChecked(item.locationId))
            {
                continue;
            }

            shown.push_back(item.locationId);

            // The client caches what it learns of an item across sessions, so an entry could still hold
            // an item of a seed played before: each one is sent again every time, to replace it.
            uint32 entry = SHOP_ITEM_ENTRY + i;
            ItemDisplay display = GetDisplay(item);
            SendItemQuery(entry, item, display);

            // Written as the core writes them: slot counted from 1, item, display, stock (-1 for unlimited),
            // price, durability, buy count, extended cost.
            list << uint32(i + 1) << entry << display.displayId << int32(-1) << GetPrice(item) << uint32(0) << uint32(1) << uint32(0);
            ++count;
        }

        if (count == 0)
        {
            ChatHandler(player->GetSession()).SendSysMessage("The shop has sold everything it had.");
            stone->OnShopSoldOut();
            return;
        }

        list.put<uint8>(8, count);
        player->SendDirectMessage(&list);

        // Seeing an item for another world tells its player nothing, so the shop hints what it shows, as
        // shops do in other Archipelago games. Those hinted already are left out, so there is usually
        // nothing left after the first opening. Opened before the server has sent the hint list after a
        // login, the shop asks for all of them again, which the server does not announce twice.
        apCharacter->HintLocations(std::move(shown));
    }

    void AP_StoneShop::OnLocationInfo(const std::list<Network::Client::NetworkItem>& items)
    {
        for (const NetworkItem& item : items)
        {
            scoutedItems[item.location] = item;
        }
    }

    bool AP_StoneShop::CanItemQueryReceive(const WorldPacket& packet)
    {
        uint32 entry = packet.read<uint32>(0);
        const Locations::ShopItem* item = GetItem(entry);
        if (!item)
        {
            return true;
        }

        SendItemQuery(entry, *item, GetDisplay(*item));
        return false;
    }

    bool AP_StoneShop::CanBuyPacketReceive(const WorldPacket& packet)
    {
        ObjectGuid vendor;
        uint32 entry;
        if (packet.GetOpcode() == CMSG_BUY_ITEM)
        {
            WorldPackets::Item::BuyItem buy{ WorldPacket(packet) };
            buy.Read();
            vendor = buy.VendorGuid;
            entry = buy.Item;
        }
        else
        {
            WorldPackets::Item::BuyItemInSlot buy{ WorldPacket(packet) };
            buy.Read();
            vendor = buy.VendorGuid;
            entry = buy.Item;
        }

        if (vendor != player->GetGUID() || !GetItem(entry))
        {
            return true;
        }

        Buy(entry);
        return false;
    }

    const Locations::ShopItem* AP_StoneShop::GetItem(uint32 entry) const
    {
        const std::vector<Locations::ShopItem>& items = apCharacter->GetLocationsContainer().shop.GetItems();
        if (entry < SHOP_ITEM_ENTRY || entry - SHOP_ITEM_ENTRY >= std::min<size_t>(items.size(), MAX_VENDOR_ITEMS))
        {
            return nullptr;
        }

        return &items[entry - SHOP_ITEM_ENTRY];
    }

    void AP_StoneShop::Buy(uint32 entry)
    {
        const Locations::ShopItem& item = *GetItem(entry);
        ChatHandler chat(player->GetSession());

        if (!apCharacter->IsSlotConnected())
        {
            chat.SendSysMessage("|cFFFF0000Not connected to the Archipelago server, nothing was bought.");
            return;
        }

        // Bought already, from a window opened before: the new one leaves it out.
        if (apCharacter->IsLocationChecked(item.locationId))
        {
            Open();
            return;
        }

        if (player->GetLevel() < item.level)
        {
            chat.SendSysMessage(fmt::format("|cFFFF0000This item goes on sale at level {}.", item.level));
            return;
        }

        uint32 price = GetPrice(item);
        if (!player->HasEnoughMoney(price))
        {
            chat.SendSysMessage("|cFFFF0000You don't have enough money.");
            return;
        }

        player->ModifyMoney(-static_cast<int32>(price));
        apCharacter->CheckLocation(item.locationId);
        Open();
    }

    AP_StoneShop::ItemDisplay AP_StoneShop::GetDisplay(const Locations::ShopItem& item) const
    {
        const Client* client = apCharacter->GetClient();
        auto scouted = scoutedItems.find(item.locationId);
        if (!client || scouted == scoutedItems.end())
        {
            return { "Archipelago Item", "What it holds is not known yet.", ITEM_QUALITY_NORMAL, GetDisplayId("INV_Misc_QuestionMark") };
        }

        const NetworkItem& held = scouted->second;
        bool own = held.player == client->GetPlayerNumber();
        std::string game = client->GetPlayerGame(held.player);
        std::string recipient = own ? std::string("you") : fmt::format("{} ({})", client->GetPlayerAlias(held.player), game);

        return {
            client->GetItemName(held.item, game),
            fmt::format("{} item for {}", GetKind(held), recipient),
            GetQuality(held),
            GetDisplayId(GetIcon(held, own))
        };
    }

    void AP_StoneShop::SendItemQuery(uint32 entry, const Locations::ShopItem& item, const ItemDisplay& display)
    {
        // Laid out as WorldSession::HandleItemQuerySingleOpcode lays out a junk item's.
        WorldPacket query(SMSG_ITEM_QUERY_SINGLE_RESPONSE, 600);
        query << entry;
        query << uint32(ITEM_CLASS_MISC) << uint32(ITEM_SUBCLASS_JUNK) << int32(-1);
        query << display.name;
        query << uint8(0) << uint8(0) << uint8(0);
        query << display.displayId << display.quality;
        query << uint32(0) << uint32(0); // flags
        query << int32(GetPrice(item)) << uint32(0); // buy and sell price
        query << uint32(INVTYPE_NON_EQUIP) << int32(-1) << int32(-1); // inventory type, classes, races
        query << uint32(0) << uint32(item.level); // item level, required level
        // Required skill and rank, spell, honor rank, city rank, reputation faction and rank
        for (int i = 0; i < 7; ++i)
        {
            query << uint32(0);
        }
        query << int32(0) << int32(1) << uint32(0) << uint32(0); // max count, stack size, container slots, stats
        query << uint32(0) << uint32(0); // scaling stats
        for (int i = 0; i < MAX_ITEM_PROTO_DAMAGES; ++i)
        {
            query << float(0) << float(0) << uint32(0);
        }
        // Armor and resistances
        for (int i = 0; i < 7; ++i)
        {
            query << uint32(0);
        }
        query << uint32(0) << uint32(0) << float(0); // delay, ammo type, ranged range
        for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            query << uint32(0) << uint32(0) << int32(0) << int32(-1) << uint32(0) << int32(-1);
        }
        query << uint32(0); // bonding
        query << display.description;
        // Page text, language, page material, start quest, lock, material, sheath, random property and
        // suffix, block, item set, durability, area, map, bag family, totem category
        for (int i = 0; i < 16; ++i)
        {
            query << uint32(0);
        }
        for (int i = 0; i < MAX_ITEM_PROTO_SOCKETS; ++i)
        {
            query << uint32(0) << uint32(0);
        }
        query << uint32(0) << uint32(0) << int32(-1); // socket bonus, gem properties, disenchant skill
        query << float(0); // armor damage modifier
        query << uint32(0) << uint32(0) << uint32(0); // duration, limit category, holiday
        player->SendDirectMessage(&query);
    }

    uint32 AP_StoneShop::GetTierPrice(uint8 level, uint8 goalLevel)
    {
        // The last tier before a level goal has no bracket after it to save up in, so it costs what the
        // tier before it does. A goal won otherwise leaves the character all the time it needs at the cap.
        if (goalLevel > 6)
        {
            level = std::min<uint8>(level, goalLevel - 6);
        }

        uint32 price = SHOP_PRICES[0].price;
        for (const ShopPrice& tier : SHOP_PRICES)
        {
            if (level >= tier.level)
            {
                price = tier.price;
            }
        }

        return price;
    }

    uint32 AP_StoneShop::GetPrice(const Locations::ShopItem& item) const
    {
        // The option for the number of items per tier sets how many checks the shop holds, not how much
        // gold it takes: the tier's price is shared out between its items.
        const std::vector<Locations::ShopItem>& items = apCharacter->GetLocationsContainer().shop.GetItems();
        uint32 tierSize = static_cast<uint32>(std::ranges::count_if(items, [&](const Locations::ShopItem& other) { return other.level == item.level; }));
        return GetTierPrice(item.level, apCharacter->GetGoalLevel()) * SHOP_PRICED_ITEMS_PER_TIER / tierSize;
    }

    uint32 AP_StoneShop::GetDisplayId(const char* icon)
    {
        // The client knows icons by display rather than by name, so the shop's icons are looked up by
        // name among the displays of the client's items, once.
        static const std::unordered_map<std::string, uint32> displays = []()
            {
                std::unordered_map<std::string, uint32> result;
                for (uint32 i = 0; i < sItemDisplayInfoStore.GetNumRows(); ++i)
                {
                    const ItemDisplayInfoEntry* display = sItemDisplayInfoStore.LookupEntry(i);
                    if (display && display->inventoryIcon && *display->inventoryIcon)
                    {
                        result.emplace(ToLower(display->inventoryIcon), display->ID);
                    }
                }

                return result;
            }();

        auto it = displays.find(ToLower(icon));
        if (it == displays.end())
        {
            it = displays.find(ToLower("INV_Misc_QuestionMark"));
        }

        return it != displays.end() ? it->second : 0;
    }
}
