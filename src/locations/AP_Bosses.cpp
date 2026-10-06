#include "Define.h"
#include "locations/AP_Bosses.h"
#include "Optional.h"

namespace ModArchipelaWoW::Locations
{
    Bosses::Bosses() :
        map()
    {
    }

    void Bosses::AddLocation(uint32 encounterId, int locationId)
    {
        map[encounterId] = locationId;
    }

    Optional<int> Bosses::GetLocationId(uint32 encounterId) const
    {
        if (map.contains(encounterId))
        {
            return map.at(encounterId);
        }

        return {};
    }
}
