#ifndef _MOD_ARCHIPELAWOW_ITEMS_HEIRLOOMS_H_
#define _MOD_ARCHIPELAWOW_ITEMS_HEIRLOOMS_H_

#include "Define.h"

#include <cstdint>
#include <initializer_list>
#include <vector>

namespace ModArchipelaWoW::Items
{
    /// The most Heirloom Weapons a slot receives, and the weapon slots the preferences store.
    constexpr uint8 MAX_HEIRLOOM_WEAPON_SLOTS = 3;

    /// An heirloom of a kind, such as "Heirloom Chest". The piece the character gets is the one for
    /// the role its preferences name when the item arrives.
    struct HeirloomPiece
    {
        int64_t apItemId = 0;
        uint32 melee = 0;
        uint32 caster = 0;
    };

    /// This slot's heirlooms. The apworld resolves them for the slot's class, so the module never
    /// needs to know which class wears what.
    struct Heirlooms
    {
        HeirloomPiece shoulders;
        HeirloomPiece chest;
        HeirloomPiece trinket;
        int64_t weapon = 0;
        /// How many Heirloom Weapons the seed holds for this slot
        uint8 weaponSlots = 0;
        /// The weapons the character can put in its slots
        std::vector<uint32> weaponChoices;

        const HeirloomPiece* GetPiece(int64_t apItemId) const
        {
            for (const HeirloomPiece* piece : { &shoulders, &chest, &trinket })
            {
                if (piece->apItemId != 0 && piece->apItemId == apItemId)
                {
                    return piece;
                }
            }

            return nullptr;
        }
    };
}

#endif
