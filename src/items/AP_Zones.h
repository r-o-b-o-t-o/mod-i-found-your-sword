#ifndef _MOD_ARCHIPELAWOW_ITEMS_ZONES_H_
#define _MOD_ARCHIPELAWOW_ITEMS_ZONES_H_

#include "Define.h"
#include "Optional.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace ModArchipelaWoW::Items
{
    struct ZoneItem
    {
    public:
        ZoneItem() : ZoneItem(0, "", 0, 0, 0.0f, 0.0f, 0.0f, 0.0f)
        {
        }

        ZoneItem(uint32 id, const std::string& icon, uint32 gossipMenu, uint32 map, float x, float y, float z, float o) :
            id(id),
            icon(icon),
            gossipMenu(gossipMenu),
            map(map),
            x(x),
            y(y),
            z(z),
            o(o)
        {
        }

        uint32 id;
        std::string icon;
        uint32 gossipMenu;
        uint32 map;
        float x;
        float y;
        float z;
        float o;
    };

    class Zones
    {
    public:
        Zones();

        void AddItem(int64_t itemId, uint32 zoneId, const std::string& icon, uint32 gossipMenu, uint32 map, float x, float y, float z, float o);
        Optional<ZoneItem> GetZone(int64_t itemId) const;
        bool IsRestricted(uint32 zoneId) const;
        auto Begin() const { return map.cbegin(); }
        auto End() const { return map.cend(); }

    private:
        std::unordered_map<int64_t, ZoneItem> map;
        std::unordered_set<uint32> zoneIds;
    };
}

#endif
