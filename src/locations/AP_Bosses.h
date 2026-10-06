#ifndef _MOD_ARCHIPELAWOW_LOCATIONS_BOSSES_H_
#define _MOD_ARCHIPELAWOW_LOCATIONS_BOSSES_H_

#include "Define.h"
#include "Optional.h"

#include <unordered_map>

namespace ModArchipelaWoW::Locations
{
    // Keyed by DungeonEncounter.dbc ID, the encounters the core credits through instance_encounters.
    class Bosses
    {
    public:
        Bosses();

        void AddLocation(uint32 encounterId, int locationId);
        Optional<int> GetLocationId(uint32 encounterId) const;

    private:
        std::unordered_map<uint32, int> map;
    };
}

#endif
