#ifndef _MOD_ARCHIPELAWOW_LOCATIONS_QUESTS_H_
#define _MOD_ARCHIPELAWOW_LOCATIONS_QUESTS_H_

#include "Define.h"
#include "Optional.h"
#include "QuestDef.h"

#include <unordered_map>

namespace ModArchipelaWoW::Locations
{
    class Quests
    {
    public:
        Quests();

        void AddLocation(uint32 questId, int locationId);
        Optional<int> GetLocationId(const Quest* quest) const;
        Optional<int> GetLocationId(uint32 questId) const;

    private:
        std::unordered_map<uint32, int> map;
    };
}

#endif
