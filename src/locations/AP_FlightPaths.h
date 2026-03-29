#ifndef _MOD_ARCHIPELAWOW_LOCATIONS_FLIGHT_PATHS_H_
#define _MOD_ARCHIPELAWOW_LOCATIONS_FLIGHT_PATHS_H_

#include "Define.h"
#include "Optional.h"

#include <unordered_map>

namespace ModArchipelaWoW::Locations
{
    class FlightPaths
    {
    public:
        FlightPaths();

        void AddLocation(uint32 nodeId, int locationId);
        Optional<int> GetLocationId(uint32 nodeId) const;

    private:
        std::unordered_map<uint32, int> map;
    };
}

#endif
