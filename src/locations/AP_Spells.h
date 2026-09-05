#ifndef _MOD_ARCHIPELAWOW_LOCATIONS_SPELLS_H_
#define _MOD_ARCHIPELAWOW_LOCATIONS_SPELLS_H_

#include "Define.h"
#include "Optional.h"

#include <unordered_map>

namespace ModArchipelaWoW::Locations
{
    class Spells
    {
    public:
        Spells();

        void AddLocation(uint32 spellId, int locationId);
        Optional<int> GetLocationId(uint32 spellId) const;

    private:
        std::unordered_map<uint32, int> map;
    };
}

#endif
