#include "Define.h"
#include "locations/AP_Shop.h"

#include <algorithm>
#include <vector>

namespace ModArchipelaWoW::Locations
{
    Shop::Shop() :
        items()
    {
    }

    void Shop::AddLocation(uint8 level, int locationId)
    {
        // The slot data is read again on every connection.
        if (std::ranges::any_of(items, [&](const ShopItem& item) { return item.locationId == locationId; }))
        {
            return;
        }

        items.push_back({ locationId, level });
    }

    const std::vector<ShopItem>& Shop::GetItems() const
    {
        return items;
    }
}
