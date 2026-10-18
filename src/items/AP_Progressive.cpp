#include "Define.h"
#include "items/AP_Progressive.h"
#include "Optional.h"
#include "SharedDefines.h"

#include <array>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

constexpr uint32 APPRENTICE_RIDING_SPELL_ID = 33388;
constexpr uint32 JOURNEYMAN_RIDING_SPELL_ID = 33391;
constexpr uint32 EXPERT_RIDING_SPELL_ID = 34090;
constexpr uint32 ARTISAN_RIDING_SPELL_ID = 34091;

namespace ModArchipelaWoW::Items
{
    Optional<ProgressiveType> ProgressiveTypeFromValue(uint32 value)
    {
        switch (value)
        {
            case static_cast<uint32>(ProgressiveType::MovementSpeed):
            case static_cast<uint32>(ProgressiveType::ExperienceRate):
            case static_cast<uint32>(ProgressiveType::Food):
            case static_cast<uint32>(ProgressiveType::Drink):
            case static_cast<uint32>(ProgressiveType::Riding):
                return static_cast<ProgressiveType>(value);
            default:
                return {};
        }
    }

    const std::vector<uint32>& GetRidingRankMounts(uint32 rankSpellId, uint8 race, TeamId team)
    {
        // The ground mounts each race's own vendor sells
        static const std::unordered_map<uint32, std::unordered_map<uint8, std::vector<uint32>>> byRace = {
            { APPRENTICE_RIDING_SPELL_ID, {
                { RACE_ORC, { 580, 6653, 6654, 64658 } },
                { RACE_TROLL, { 8395, 10796, 10799 } },
                { RACE_TAUREN, { 18989, 18990, 64657 } },
                { RACE_UNDEAD_PLAYER, { 64977, 17462, 17463, 17464 } },
                { RACE_BLOODELF, { 34795, 35020, 35022, 35018 } },
                { RACE_HUMAN, { 472, 6648, 458, 470 } },
                { RACE_DWARF, { 6777, 6899, 6898 } },
                { RACE_NIGHTELF, { 66847, 10793, 8394, 10789 } },
                { RACE_GNOME, { 17454, 10873, 10969, 17453 } },
                { RACE_DRAENEI, { 34406, 35711, 35710 } },
            } },
            { JOURNEYMAN_RIDING_SPELL_ID, {
                { RACE_ORC, { 23252, 23250, 23251 } },
                { RACE_TROLL, { 23243, 23241, 23242 } },
                { RACE_TAUREN, { 23248, 23247, 23249 } },
                { RACE_UNDEAD_PLAYER, { 66846, 17465, 23246 } },
                { RACE_BLOODELF, { 35027, 33660, 35025 } },
                { RACE_HUMAN, { 23227, 23229, 23228 } },
                { RACE_DWARF, { 23239, 23240, 23238 } },
                { RACE_NIGHTELF, { 23221, 23219, 23338 } },
                { RACE_GNOME, { 23222, 23225, 23223 } },
                { RACE_DRAENEI, { 35713, 35712, 35714 } },
            } },
        };
        // The flying mounts are sold by faction rather than by race, indexed by TeamId
        static const std::unordered_map<uint32, std::array<std::vector<uint32>, 2>> byTeam = {
            { EXPERT_RIDING_SPELL_ID, { {
                { 32235, 32239, 32240 },
                { 32243, 32244, 32245 },
            } } },
            { ARTISAN_RIDING_SPELL_ID, { {
                { 32242, 32289, 32290, 32292 },
                { 32246, 32295, 32296, 32297 },
            } } },
        };
        static const std::vector<uint32> none;

        if (auto rank = byRace.find(rankSpellId); rank != byRace.end())
        {
            auto mounts = rank->second.find(race);
            return mounts != rank->second.end() ? mounts->second : none;
        }

        if (auto rank = byTeam.find(rankSpellId); rank != byTeam.end() && team != TEAM_NEUTRAL)
        {
            return rank->second[team];
        }

        return none;
    }

    Progressive::Progressive() :
        map(),
        byType()
    {
    }

    void Progressive::AddItem(int64_t apItemId, ProgressiveType type, std::vector<uint32> steps)
    {
        ProgressiveItem item(type, std::move(steps));
        byType[type] = item;
        map[apItemId] = std::move(item);
    }

    Optional<ProgressiveItem> Progressive::GetProgressiveItem(int64_t apItemId) const
    {
        if (map.contains(apItemId))
        {
            return map.at(apItemId);
        }

        return {};
    }

    const ProgressiveItem* Progressive::GetProgressiveItemByType(ProgressiveType type) const
    {
        auto item = byType.find(type);
        if (item == byType.end())
        {
            return nullptr;
        }

        return &item->second;
    }
}
