#ifndef _MOD_ARCHIPELAWOW_LOCATIONS_EXPLORATIONS_H_
#define _MOD_ARCHIPELAWOW_LOCATIONS_EXPLORATIONS_H_

#include "Define.h"
#include "Optional.h"

#include <unordered_map>

namespace ModArchipelaWoW::Locations
{
    // Keyed by Achievement_Criteria.dbc ID, the subzones of the exploration achievements.
    class Explorations
    {
    public:
        Explorations();

        void AddLocation(uint32 criteriaId, int locationId);
        Optional<int> GetLocationId(uint32 criteriaId) const;

    private:
        std::unordered_map<uint32, int> map;
    };
}

#endif
