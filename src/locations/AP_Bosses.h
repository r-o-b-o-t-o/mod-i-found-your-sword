#ifndef _MOD_ARCHIPELAWOW_LOCATIONS_BOSSES_H_
#define _MOD_ARCHIPELAWOW_LOCATIONS_BOSSES_H_

#include "Define.h"
#include "Optional.h"

#include <array>
#include <unordered_map>

namespace ModArchipelaWoW::Locations
{
    struct SyntheticEncounter
    {
        uint32 creatureEntry;
        uint32 mapId;
        uint32 encounterId;
    };

    // Encounters with no DungeonEncounter.dbc entry for the core to credit: the module credits these
    // made-up encounter ids, past every DBC id, when the creature dies on that map. The APWorld's
    // bosses.json carries the same ids.
    constexpr auto SYNTHETIC_ENCOUNTERS = std::to_array<SyntheticEncounter>({
        { 7604, 209, 100000 }, // Sergeant Bly's crew, Zul'Farrak
        { 7023, 70, 100001 },  // Obsidian Sentinel, Uldaman
    });

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
