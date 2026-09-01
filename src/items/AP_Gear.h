#ifndef _MOD_ARCHIPELAWOW_ITEMS_GEAR_H_
#define _MOD_ARCHIPELAWOW_ITEMS_GEAR_H_

#include "Define.h"
#include "Optional.h"

#include <cstdint>
#include <unordered_map>

namespace ModArchipelaWoW::Items
{
    /// The buckets an Archipelago gear item rolls from. Shields and off-hand holdables count as
    /// weapons, and relics -- librams, idols, totems, sigils -- as jewellery, which is how a player
    /// thinks of them even though the game files file every one of them under armor.
    enum class GearCategory : uint8
    {
        Weapon      = 1,
        Armor       = 2,
        Jewellery   = 3,

        Count
    };

    Optional<GearCategory> GearCategoryFromValue(uint32 value);

    struct GearItem
    {
    public:
        GearItem() : GearItem(GearCategory::Weapon, 0)
        {
        }

        GearItem(GearCategory category, uint32 quality) :
            category(category),
            quality(quality)
        {
        }

        GearCategory category;
        uint32 quality;
    };

    /// Maps the Archipelago item ids of this slot's gear items -- "Random Rare Weapon" and the rest
    /// -- to the bucket each one draws from. Which gear piece the player actually gets is decided
    /// later, by GearPool, out of what the character can wear at the level it is when the item lands.
    class Gear
    {
    public:
        Gear();

        void AddItem(int64_t apItemId, GearCategory category, uint32 quality);
        Optional<GearItem> GetGearItem(int64_t apItemId) const;

    private:
        std::unordered_map<int64_t, GearItem> map;
    };
}

#endif
