#include "Define.h"
#include "locations/AP_Spells.h"
#include "Optional.h"

namespace ModArchipelaWoW::Locations
{
    Spells::Spells() :
        map()
    {
    }

    void Spells::AddLocation(uint32 spellId, int locationId)
    {
        map[spellId] = locationId;
    }

    Optional<int> Spells::GetLocationId(uint32 spellId) const
    {
        if (map.contains(spellId))
        {
            return map.at(spellId);
        }

        return {};
    }
}
