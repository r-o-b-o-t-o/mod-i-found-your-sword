#ifndef _MOD_ARCHIPELAWOW_ITEMS_PROGRESSIVE_H_
#define _MOD_ARCHIPELAWOW_ITEMS_PROGRESSIVE_H_

#include "Define.h"
#include "Optional.h"

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ModArchipelaWoW::Items
{
    /// The tracks an Archipelago progressive item upgrades. A seed hands the same item id out
    /// several times and every copy moves its track one step further along, so a later copy keeps
    /// paying off instead of going to waste.
    enum class ProgressiveType : uint8
    {
        MovementSpeed   = 1,
        ExperienceRate  = 2,
        Food            = 3,
        Drink           = 4,

        Count
    };

    Optional<ProgressiveType> ProgressiveTypeFromValue(uint32 value);

    struct ProgressiveItem
    {
    public:
        ProgressiveItem() : ProgressiveItem(ProgressiveType::MovementSpeed, {})
        {
        }

        ProgressiveItem(ProgressiveType type, std::vector<uint32> steps) :
            type(type),
            steps(std::move(steps))
        {
        }

        /// What this track is worth once `count` copies have arrived. What the number means is up
        /// to the track: a percentage for the ones that scale a rate, a WoW item id for the ones
        /// that hand over an item. Copies past the last step -- which a seed should never hand out
        /// -- hold on the last step rather than running off the end of the list.
        uint32 ValueAt(uint32 count) const
        {
            if (count == 0 || steps.empty())
            {
                return 0;
            }

            return steps[std::min<size_t>(count, steps.size()) - 1];
        }

        ProgressiveType type;
        std::vector<uint32> steps;
    };

    /// Maps the Archipelago item ids of this slot's progressive items to the track each one
    /// upgrades and to what every step along it is worth. The seed decides both, so a slot that
    /// rolled no copies of a track simply never registers it.
    class Progressive
    {
    public:
        Progressive();

        void AddItem(int64_t apItemId, ProgressiveType type, std::vector<uint32> steps);
        Optional<ProgressiveItem> GetProgressiveItem(int64_t apItemId) const;
        /// A pointer rather than an Optional: GetProgressiveStep() calls this on every experience
        /// gain, and returning by value would copy the steps vector -- a heap allocation per kill.
        const ProgressiveItem* GetProgressiveItemByType(ProgressiveType type) const;

    private:
        std::unordered_map<int64_t, ProgressiveItem> map;
        std::unordered_map<ProgressiveType, ProgressiveItem> byType;
    };
}

#endif
