#include "Define.h"
#include "locations/AP_Levels.h"
#include "Optional.h"

namespace ModArchipelaWoW::Locations
{
    Levels::Levels() :
        map()
    {
    }

    void Levels::AddLocation(uint8 level, int locationId)
    {
        map[level] = locationId;
    }

    Optional<int> Levels::GetLocationId(uint8 level) const
    {
        if (map.contains(level))
        {
            return map.at(level);
        }

        return {};
    }
}
