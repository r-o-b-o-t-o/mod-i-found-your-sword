#include "Define.h"
#include "locations/AP_Explorations.h"
#include "Optional.h"

#include <unordered_map>

namespace ModArchipelaWoW::Locations
{
    Explorations::Explorations() :
        map()
    {
    }

    void Explorations::AddLocation(uint32 criteriaId, int locationId)
    {
        map[criteriaId] = locationId;
    }

    Optional<int> Explorations::GetLocationId(uint32 criteriaId) const
    {
        if (map.contains(criteriaId))
        {
            return map.at(criteriaId);
        }

        return {};
    }
}
