#include "Define.h"
#include "locations/AP_Quests.h"
#include "Optional.h"
#include "QuestDef.h"

namespace ModArchipelaWoW::Locations
{
    Quests::Quests() :
        map()
    {
    }

    void Quests::AddLocation(uint32 questId, int locationId)
    {
        map[questId] = locationId;
    }

    Optional<int> Quests::GetLocationId(const Quest* quest) const
    {
        if (!quest)
        {
            return {};
        }

        return GetLocationId(quest->GetQuestId());
    }

    Optional<int> Quests::GetLocationId(uint32 questId) const
    {
        if (map.contains(questId))
        {
            return map.at(questId);
        }

        return {};
    }
}
