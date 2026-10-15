#ifndef _MOD_ARCHIPELAWOW_ITEMS_TRAPS_H_
#define _MOD_ARCHIPELAWOW_ITEMS_TRAPS_H_

#include "Define.h"
#include "Optional.h"

#include <cstdint>
#include <string>
#include <unordered_map>

namespace ModArchipelaWoW::Items
{
    /// Saved in the preferences table while a trap waits to spring, so the values must not change.
    enum class TrapType : uint8
    {
        Dazed = 1,
        Shackle = 2,
        Sheep = 3,
        Poison = 4,
        Teleport = 5,
        Ambush = 6,
        Disarm = 7,
        Bombs = 8,
    };

    /// The keys the apworld sends the trap items under.
    inline Optional<TrapType> TrapTypeFromKey(const std::string& key)
    {
        static const std::unordered_map<std::string, TrapType> keys = {
            { "dazed", TrapType::Dazed },
            { "shackle", TrapType::Shackle },
            { "sheep", TrapType::Sheep },
            { "poison", TrapType::Poison },
            { "teleport", TrapType::Teleport },
            { "ambush", TrapType::Ambush },
            { "disarm", TrapType::Disarm },
            { "bombs", TrapType::Bombs },
        };

        auto it = keys.find(key);
        if (it == keys.end())
        {
            return {};
        }
        return it->second;
    }

    inline Optional<TrapType> TrapTypeFromValue(uint32 value)
    {
        if (value < static_cast<uint32>(TrapType::Dazed) || value > static_cast<uint32>(TrapType::Bombs))
        {
            return {};
        }
        return static_cast<TrapType>(value);
    }

    struct Traps
    {
        std::unordered_map<int64_t, TrapType> items;

        Optional<TrapType> GetTrap(int64_t itemId) const
        {
            auto it = items.find(itemId);
            if (it == items.end())
            {
                return {};
            }
            return it->second;
        }
    };
}

#endif
