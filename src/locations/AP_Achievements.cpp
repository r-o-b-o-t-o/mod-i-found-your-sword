#include "DBCStructure.h"
#include "Define.h"
#include "locations/AP_Achievements.h"
#include "Optional.h"

namespace ModArchipelaWoW::Locations
{
    Achievements::Achievements() :
        map()
    {
    }

    void Achievements::AddLocation(uint32 achievementId, int locationId)
    {
        map[achievementId] = locationId;
    }

    Optional<int> Achievements::GetLocationId(const AchievementEntry* achievement) const
    {
        if (!achievement)
        {
            return {};
        }

        return GetLocationId(achievement->ID);
    }

    Optional<int> Achievements::GetLocationId(uint32 achievementId) const
    {
        if (map.contains(achievementId))
        {
            return map.at(achievementId);
        }

        return {};
    }
}
