#ifndef _MOD_ARCHIPELAWOW_LOCATIONS_LEVELS_H_
#define _MOD_ARCHIPELAWOW_LOCATIONS_LEVELS_H_

#include "Define.h"
#include "Optional.h"

#include <unordered_map>

namespace ModArchipelaWoW::Locations
{
    class Levels
    {
    public:
        Levels();

        void AddLocation(uint8 level, int locationId);
        Optional<int> GetLocationId(uint8 level) const;

    private:
        std::unordered_map<uint8, int> map;
    };
}

#endif
