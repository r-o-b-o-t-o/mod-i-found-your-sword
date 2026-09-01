#ifndef _MOD_ARCHIPELAWOW_ITEMS_ITEMS_CONTAINER_H_
#define _MOD_ARCHIPELAWOW_ITEMS_ITEMS_CONTAINER_H_

#include "items/AP_Gear.h"
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
            gear(),
            zones(),
            levels(0),
            goldPouch(0)
        {
        }

        Items items;
        Gear gear;
        Zones zones;
        int64_t levels;
        int64_t goldPouch;
    };
}

#endif
