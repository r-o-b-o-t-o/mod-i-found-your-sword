#ifndef _MOD_ARCHIPELAWOW_LOCATIONS_LOCATIONS_CONTAINER_H_
#define _MOD_ARCHIPELAWOW_LOCATIONS_LOCATIONS_CONTAINER_H_

#include "locations/AP_Achievements.h"
#include "locations/AP_Bosses.h"
#include "locations/AP_Explorations.h"
#include "locations/AP_FlightPaths.h"
#include "locations/AP_Levels.h"
#include "locations/AP_Quests.h"
#include "locations/AP_Skills.h"
#include "locations/AP_Spells.h"

namespace ModArchipelaWoW::Locations
{
    class LocationsContainer
    {
    public:
        LocationsContainer() :
            achievements(),
            bosses(),
            explorations(),
            flightPaths(),
            levels(),
            quests(),
            skills(),
            spells()
        {
        }

        Achievements achievements;
        Bosses bosses;
        Explorations explorations;
        FlightPaths flightPaths;
        Levels levels;
        Quests quests;
        Skills skills;
        Spells spells;
    };
}

#endif
