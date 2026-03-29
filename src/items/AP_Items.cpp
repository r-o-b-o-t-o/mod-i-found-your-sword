#include "Define.h"
#include "items/AP_Items.h"
#include "Optional.h"

#include <cstdint>

namespace ModArchipelaWoW::Items
{
    Items::Items() :
        map()
    {
    }

    void Items::AddItem(int64_t apItemId, uint32 wowItemId)
    {
        map[apItemId] = wowItemId;
    }

    Optional<uint32> Items::GetWoWItemId(int64_t apItemId) const
    {
        if (map.contains(apItemId))
        {
            return map.at(apItemId);
        }

        return {};
    }
}
