#ifndef _MOD_ARCHIPELAWOW_LOCATIONS_LEADERS_H_
#define _MOD_ARCHIPELAWOW_LOCATIONS_LEADERS_H_

#include "Define.h"
#include "Optional.h"

#include <unordered_map>

namespace ModArchipelaWoW::Locations
{
    // Keyed by creature entry, the faction leaders greeted with /hi or /salute.
    class Leaders
    {
    public:
        Leaders();

        void AddLocation(uint32 creatureId, int locationId);
        Optional<int> GetLocationId(uint32 creatureId) const;

    private:
        std::unordered_map<uint32, int> map;
    };
}

#endif
