#ifndef _MOD_ARCHIPELAWOW_ITEMS_GEAR_POOL_H_
#define _MOD_ARCHIPELAWOW_ITEMS_GEAR_POOL_H_

#include "Define.h"
#include "items/AP_Gear.h"
#include "ItemTemplate.h"
#include "Optional.h"
#include "Player.h"

#include <array>
#include <cstddef>
#include <unordered_set>
#include <vector>

namespace ModArchipelaWoW::Items
{
    /// Every uncommon and rare gear piece the server can hand out, bucketed by category, quality and
    /// required level. Rolling a reward then only has to walk the handful of level bands around the
    /// character instead of the whole 46k-entry item template store.
    ///
    /// Built once at startup. Worldserver loads item templates a single time and never reloads them,
    /// so the ItemTemplate pointers stay good for the lifetime of the process.
    class GearPool
    {
    public:
        GearPool();

        void Build();

        /// Picks a gear piece the character can wear, requiring a level within `levelWindow` of the
        /// character's own where possible and never above `goalMaxLevel`, the level the run itself
        /// stops at. Unless `allArmorTypes` is set, armor is narrowed to the heaviest types the class
        /// is up to. Returns nothing only when the pool holds nothing at all this class could ever
        /// use, which the six shipped categories never hit.
        Optional<uint32> Roll(GearCategory category, uint32 quality, Player const* player,
            uint8 levelWindow, bool allArmorTypes, uint8 goalMaxLevel) const;

    private:
        // Only uncommon and rare gear is ever handed out, so the quality axis is two buckets wide
        // rather than the seven ITEM_QUALITY_* values.
        static constexpr std::size_t QUALITY_COUNT = 2;
        // How far the level window grows on each pass when it comes up empty. Deliberately small:
        // the gear levels are sparse at the bottom of the range -- nothing at all below required
        // level 5, and no rare below 13 -- so a wide step would overshoot a narrow window by a
        // decade rather than creep to the nearest level that actually holds something.
        static constexpr int32 LEVEL_WINDOW_WIDEN_STEP = 3;
        // How many candidates the window has to turn up before it stops growing. Stopping at the
        // first one found leaves the sparse corners of the pool handing out the same piece every
        // time -- at level 1 there is exactly one uncommon ring in range -- so the window keeps
        // opening until there is something to choose between.
        static constexpr std::size_t LEVEL_WINDOW_MIN_CANDIDATES = 5;

        /// One SkillRaceClassInfo.dbc row, reduced to the level at which the races and classes it
        /// covers may first train the skill.
        struct SkillTraining
        {
            uint32 skillId;
            uint32 raceMask;
            uint32 classMask;
            uint8 minLevel;
        };

        using LevelBucket = std::vector<ItemTemplate const*>;
        using QualityBuckets = std::array<std::vector<LevelBucket>, QUALITY_COUNT>;

        std::array<QualityBuckets, static_cast<std::size_t>(GearCategory::Count)> buckets;
        std::vector<SkillTraining> skillTrainings;
        uint8 maxLevel;

        bool IsUsableBy(ItemTemplate const* item, Player const* player) const;
        uint8 TrainingLevel(uint32 skill, Player const* player) const;
        uint32 AllowedArmorMask(Player const* player, uint8 levelWindow, int32 levelCeiling) const;
        void LoadSkillTrainings();

        static bool MatchesArmorMask(ItemTemplate const* item, uint32 allowedMask);

        static Optional<std::size_t> QualityIndex(uint32 quality);
        static Optional<GearCategory> CategoryOf(ItemTemplate const* item);
        static std::unordered_set<uint32> LoadObtainableItems();
    };
}

#endif
