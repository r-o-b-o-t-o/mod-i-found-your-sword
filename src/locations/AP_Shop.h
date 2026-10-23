#ifndef _MOD_ARCHIPELAWOW_LOCATIONS_SHOP_H_
#define _MOD_ARCHIPELAWOW_LOCATIONS_SHOP_H_

#include "Define.h"

#include <vector>

namespace ModArchipelaWoW::Locations
{
    struct ShopItem
    {
        int locationId;
        uint8 level; // The level its tier goes on sale at
    };

    // The Archipelago Stone's shop, in the order it lists its items: by tier, then by slot.
    class Shop
    {
    public:
        Shop();

        void AddLocation(uint8 level, int locationId);
        const std::vector<ShopItem>& GetItems() const;

    private:
        std::vector<ShopItem> items;
    };
}

#endif
