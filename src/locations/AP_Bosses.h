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

    // The Sunken Temple's six troll defenders, whose deaths open the forcefield before Jammal'an the Prophet,
    // make one more made-up encounter, credited when the last of them dies. The core's instance script counts
    // their deaths and saves the count with the instance (DATA_DEFENDER_KILLED in sunken_temple.h), so the list
    // must hold exactly its DEFENDERS_COUNT.
    constexpr uint32 SUNKEN_TEMPLE_MAP_ID = 109;
    constexpr uint32 SUNKEN_TEMPLE_DATA_DEFENDER_KILLED = 11;
    constexpr uint32 ATALAI_DEFENDERS_ENCOUNTER_ID = 100002;
    // Zolo, Gasher, Loro, Hukku, Zul'Lor, Mijan
    constexpr auto ATALAI_DEFENDERS = std::to_array<uint32>({ 5712, 5713, 5714, 5715, 5716, 5717 });

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
