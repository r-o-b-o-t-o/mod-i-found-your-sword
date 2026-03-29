#include "Define.h"
#include "locations/AP_GameObjects.h"
#include "Optional.h"

namespace ModArchipelaWoW::Locations
{
    GameObjects::GameObjects() :
        map()
    {
    }

    void GameObjects::AddLocation(uint32 questId, int locationId)
    {
        map[questId] = locationId;
    }

    Optional<int> GameObjects::GetLocationId(uint32 questId) const
    {
        if (map.contains(questId))
        {
            return map.at(questId);
        }

        return {};
    }
}
