#include "Define.h"
#include "locations/AP_Skills.h"

#include <map>
#include <unordered_map>
#include <vector>

namespace ModArchipelaWoW::Locations
{
    Skills::Skills() :
        map()
    {
    }

    void Skills::AddLocation(uint32 skillId, uint32 value, int locationId)
    {
        map[skillId][value] = locationId;
    }

    std::vector<int> Skills::GetLocationIds(uint32 skillId, uint32 value) const
    {
        std::vector<int> locationIds;
        auto skill = map.find(skillId);
        if (skill == map.end())
        {
            return locationIds;
        }

        for (auto itr = skill->second.begin(); itr != skill->second.upper_bound(value); ++itr)
        {
            locationIds.push_back(itr->second);
        }

        return locationIds;
    }

    const std::unordered_map<uint32, std::map<uint32, int>>& Skills::GetLocations() const
    {
        return map;
    }
}
