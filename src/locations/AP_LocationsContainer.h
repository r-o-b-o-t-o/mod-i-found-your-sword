#ifndef _MOD_ARCHIPELAWOW_LOCATIONS_LOCATIONS_CONTAINER_H_
#define _MOD_ARCHIPELAWOW_LOCATIONS_LOCATIONS_CONTAINER_H_

#include "locations/AP_Achievements.h"
#include "locations/AP_FlightPaths.h"
#include "locations/AP_Levels.h"
#include "locations/AP_Quests.h"
#include "locations/AP_Spells.h"

namespace ModArchipelaWoW::Locations
{
    class LocationsContainer
    {
    public:
        LocationsContainer() :
            achievements(),
            flightPaths(),
            levels(),
            quests(),
            spells()
        {
        }

        Achievements achievements;
        FlightPaths flightPaths;
        Levels levels;
        Quests quests;
        Spells spells;
    };
}

#endif
