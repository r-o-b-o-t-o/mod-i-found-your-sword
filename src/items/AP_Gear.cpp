#include "Define.h"
#include "items/AP_Gear.h"
#include "Optional.h"

#include <cstdint>

namespace ModArchipelaWoW::Items
{
    Optional<GearCategory> GearCategoryFromValue(uint32 value)
    {
        switch (value)
        {
            case static_cast<uint32>(GearCategory::Weapon):
            case static_cast<uint32>(GearCategory::Armor):
            case static_cast<uint32>(GearCategory::Jewellery):
                return static_cast<GearCategory>(value);
            default:
                return {};
        }
    }

    Gear::Gear() :
        map()
    {
    }

    void Gear::AddItem(int64_t apItemId, GearCategory category, uint32 quality)
    {
        map[apItemId] = GearItem(category, quality);
    }

    Optional<GearItem> Gear::GetGearItem(int64_t apItemId) const
    {
        if (map.contains(apItemId))
        {
            return map.at(apItemId);
        }

        return {};
    }
}
