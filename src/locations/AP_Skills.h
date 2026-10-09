#ifndef _MOD_ARCHIPELAWOW_LOCATIONS_SKILLS_H_
#define _MOD_ARCHIPELAWOW_LOCATIONS_SKILLS_H_

#include "Define.h"

#include <map>
#include <unordered_map>
#include <vector>

namespace ModArchipelaWoW::Locations
{
    // Keyed by SkillLine.dbc ID, then by skill value: the values of the weapon skills and Defense that are locations.
    class Skills
    {
    public:
        Skills();

        void AddLocation(uint32 skillId, uint32 value, int locationId);
        /// The locations of a skill that a value reaches: its own and every one below it.
        std::vector<int> GetLocationIds(uint32 skillId, uint32 value) const;
        const std::unordered_map<uint32, std::map<uint32, int>>& GetLocations() const;

    private:
        std::unordered_map<uint32, std::map<uint32, int>> map;
    };
}

#endif
