#ifndef _MOD_ARCHIPELAWOW_ITEMS_ITEMS_CONTAINER_H_
#define _MOD_ARCHIPELAWOW_ITEMS_ITEMS_CONTAINER_H_

#include "items/AP_Items.h"
#include "items/AP_Zones.h"

#include <cstdint>

namespace ModArchipelaWoW::Items
{
    class ItemsContainer
    {
    public:
        ItemsContainer() :
            items(),
            zones(),
            levels(0)
        {
        }

        Items items;
        Zones zones;
        int64_t levels;
    };
}

#endif
