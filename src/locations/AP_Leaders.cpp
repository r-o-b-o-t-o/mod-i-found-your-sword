#include "Define.h"
#include "locations/AP_Leaders.h"
#include "Optional.h"

namespace ModArchipelaWoW::Locations
{
    Leaders::Leaders() :
        map()
    {
    }

    void Leaders::AddLocation(uint32 creatureId, int locationId)
    {
        map[creatureId] = locationId;
    }

    Optional<int> Leaders::GetLocationId(uint32 creatureId) const
    {
        if (map.contains(creatureId))
        {
            return map.at(creatureId);
        }

        return {};
    }
}
