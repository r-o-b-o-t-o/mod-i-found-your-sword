#include "Define.h"
#include "items/AP_Progressive.h"
#include "Optional.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace ModArchipelaWoW::Items
{
    Optional<ProgressiveType> ProgressiveTypeFromValue(uint32 value)
    {
        switch (value)
        {
            case static_cast<uint32>(ProgressiveType::MovementSpeed):
            case static_cast<uint32>(ProgressiveType::ExperienceRate):
            case static_cast<uint32>(ProgressiveType::Food):
            case static_cast<uint32>(ProgressiveType::Drink):
                return static_cast<ProgressiveType>(value);
            default:
                return {};
        }
    }

    Progressive::Progressive() :
        map(),
        byType()
    {
    }

    void Progressive::AddItem(int64_t apItemId, ProgressiveType type, std::vector<uint32> steps)
    {
        ProgressiveItem item(type, std::move(steps));
        byType[type] = item;
        map[apItemId] = std::move(item);
    }

    Optional<ProgressiveItem> Progressive::GetProgressiveItem(int64_t apItemId) const
    {
        if (map.contains(apItemId))
        {
            return map.at(apItemId);
        }

        return {};
    }

    const ProgressiveItem* Progressive::GetProgressiveItemByType(ProgressiveType type) const
    {
        auto item = byType.find(type);
        if (item == byType.end())
        {
            return nullptr;
        }

        return &item->second;
    }
}
