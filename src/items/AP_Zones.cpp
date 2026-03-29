#include "Define.h"
#include "items/AP_Zones.h"
#include "Optional.h"

#include <cstdint>
#include <string>

namespace ModArchipelaWoW::Items
{
    Zones::Zones() :
        map(),
        zoneIds()
    {
    }

    void Zones::AddItem(int64_t itemId, uint32 zoneId, const std::string& icon, uint32 gossipMenu, uint32 mapId, float x, float y, float z, float o)
    {
        map[itemId] = ZoneItem(zoneId, icon, gossipMenu, mapId, x, y, z, o);
        zoneIds.insert(zoneId);
    }

    Optional<ZoneItem> Zones::GetZone(int64_t itemId) const
    {
        if (map.contains(itemId))
        {
            return map.at(itemId);
        }

        return {};
    }

    bool Zones::IsRestricted(uint32 zoneId) const
    {
        return zoneIds.contains(zoneId);
    }
}
