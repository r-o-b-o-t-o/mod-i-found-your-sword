#ifndef _MOD_ARCHIPELAWOW_LOCATIONS_BOSSES_H_
#define _MOD_ARCHIPELAWOW_LOCATIONS_BOSSES_H_

#include "Define.h"
#include "Optional.h"

#include <unordered_map>

namespace ModArchipelaWoW::Locations
{
    // Sergeant Bly's crew in Zul'Farrak has no DungeonEncounter.dbc entry for the core to credit, so the
    // module credits this made-up encounter id, past every DBC id, when Bly dies. The APWorld's
    // bosses.json carries the same id.
    constexpr uint32 SERGEANT_BLY_ENCOUNTER_ID = 100000;
    constexpr uint32 SERGEANT_BLY_ENTRY = 7604;
    constexpr uint32 ZULFARRAK_MAP_ID = 209;

    // Keyed by DungeonEncounter.dbc ID, the encounters the core credits through instance_encounters, or by
    // one of the made-up ids above.
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
