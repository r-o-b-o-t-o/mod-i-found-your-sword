#ifndef _MOD_ARCHIPELAWOW_ITEMS_ITEMS_H_
#define _MOD_ARCHIPELAWOW_ITEMS_ITEMS_H_

#include "Define.h"
#include "Optional.h"

#include <cstdint>
#include <unordered_map>

namespace ModArchipelaWoW::Items
{
    class Items
    {
    public:
        Items();

        void AddItem(int64_t apItemId, uint32 wowItemId);
        Optional<uint32> GetWoWItemId(int64_t apItemId) const;

    private:
        std::unordered_map<int64_t, uint32> map;
    };
}

#endif
