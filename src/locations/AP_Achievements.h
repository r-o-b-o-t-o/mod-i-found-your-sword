#ifndef _MOD_ARCHIPELAWOW_LOCATIONS_ACHIEVEMENTS_H_
#define _MOD_ARCHIPELAWOW_LOCATIONS_ACHIEVEMENTS_H_

#include "DBCStructure.h"
#include "Define.h"
#include "Optional.h"

#include <unordered_map>

namespace ModArchipelaWoW::Locations
{
    class Achievements
    {
    public:
        Achievements();

        void AddLocation(uint32 achievementId, int locationId);
        Optional<int> GetLocationId(const AchievementEntry* achievement) const;
        Optional<int> GetLocationId(uint32 achievementId) const;

    private:
        std::unordered_map<uint32, int> map;
    };
}

#endif
