#ifndef _MOD_ARCHIPELAWOW_ITEMS_WORLD_BUFFS_H_
#define _MOD_ARCHIPELAWOW_ITEMS_WORLD_BUFFS_H_

#include "Define.h"

#include <cstdint>
#include <vector>

namespace ModArchipelaWoW::Items
{
    /// The Random World Buff item and the spells it draws from, for each role. The apworld resolves
    /// them for the slot's class, like the heirlooms.
    struct WorldBuffs
    {
        int64_t apItemId = 0;
        std::vector<uint32> melee;
        std::vector<uint32> caster;
    };
}

#endif
