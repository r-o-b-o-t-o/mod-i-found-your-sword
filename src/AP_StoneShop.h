#ifndef _MOD_ARCHIPELAWOW_AP_STONE_SHOP_H_
#define _MOD_ARCHIPELAWOW_AP_STONE_SHOP_H_

#include "Define.h"
#include "locations/AP_Shop.h"
#include "network/AP_Client.h"
#include "Player.h"
#include "WorldPacket.h"

#include <list>
#include <string>
#include <unordered_map>

namespace ModArchipelaWoW
{
    class AP_Character;
    class AP_Stone;

    /// The Archipelago Stone's shop: the seed's shop locations, sold for gold in a merchant window.
    class AP_StoneShop
    {
    public:
        AP_StoneShop(AP_Character* apCharacter, AP_Stone* stone);

        /// Whether any of the seed's shop items is still for sale.
        bool HasItemsLeft() const;
        void Open();
        void OnLocationInfo(const std::list<Network::Client::NetworkItem>& items);
        /// Answers the client's queries for the shop's items. False when it did.
        bool CanItemQueryReceive(const WorldPacket& packet);
        /// Takes over the purchases of the shop's items. False when it did.
        bool CanBuyPacketReceive(const WorldPacket& packet);
        /// What one of the tier on sale at a level costs in a shop of three items per tier. The Gold
        /// Pouch is measured against it.
        static uint32 GetTierPrice(uint8 level, uint8 goalLevel);

    private:
        struct ItemDisplay
        {
            std::string name;
            std::string description;
            uint32 quality;
            uint32 displayId;
        };

        AP_Character* apCharacter;
        AP_Stone* stone;
        Player* player;
        /// What the locations the server scouted hold, by location: the shop's, and those a Scroll of
        /// Divination hinted.
        std::unordered_map<int64, Network::Client::NetworkItem> scoutedItems;

        const Locations::ShopItem* GetItem(uint32 entry) const;
        void Buy(uint32 entry);
        ItemDisplay GetDisplay(const Locations::ShopItem& item) const;
        void SendItemQuery(uint32 entry, const Locations::ShopItem& item, const ItemDisplay& display);
        uint32 GetPrice(const Locations::ShopItem& item) const;
        static uint32 GetDisplayId(const char* icon);
    };
}

#endif
