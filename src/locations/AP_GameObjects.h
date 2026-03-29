#ifndef _MOD_ARCHIPELAWOW_LOCATIONS_GAME_OBJECTS_H_
#define _MOD_ARCHIPELAWOW_LOCATIONS_GAME_OBJECTS_H_

#include "Define.h"
#include "Optional.h"

#include <unordered_map>

namespace ModArchipelaWoW::Locations
{
    class GameObjects
    {
    public:
        GameObjects();

        void AddLocation(uint32 entry, int locationId);
        Optional<int> GetLocationId(uint32 entry) const;

    private:
        std::unordered_map<uint32, int> map;
    };
}

#endif
