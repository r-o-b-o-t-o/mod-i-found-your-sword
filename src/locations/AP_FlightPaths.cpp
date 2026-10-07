#include "Define.h"
#include "locations/AP_FlightPaths.h"
#include "Optional.h"

namespace ModArchipelaWoW::Locations
{
    FlightPaths::FlightPaths() :
        map()
    {
    }

    void FlightPaths::AddLocation(uint32 nodeId, int locationId)
    {
        map[nodeId] = locationId;
    }

    Optional<int> FlightPaths::GetLocationId(uint32 nodeId) const
    {
        if (map.contains(nodeId))
        {
            return map.at(nodeId);
        }

        return {};
    }

    const std::unordered_map<uint32, int>& FlightPaths::GetLocations() const
    {
        return map;
    }
}
