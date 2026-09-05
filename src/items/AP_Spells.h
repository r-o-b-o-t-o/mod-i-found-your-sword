#ifndef _MOD_ARCHIPELAWOW_ITEMS_SPELLS_H_
#define _MOD_ARCHIPELAWOW_ITEMS_SPELLS_H_

#include "Define.h"
#include "Optional.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ModArchipelaWoW::Items
{
    struct SpellItem
    {
    public:
        SpellItem() : SpellItem(0, 0, 0, {})
        {
        }

        SpellItem(int64_t itemId, uint32 spellId, uint8 reqLevel, std::vector<uint32> taughtSpells) :
            itemId(itemId),
            spellId(spellId),
            reqLevel(reqLevel),
            taughtSpells(std::move(taughtSpells))
        {
        }

        int64_t itemId;
        uint32 spellId;
        /// The level the trainer that teaches this spell asks for. The module needs it because the
        /// trainer works the state of a spell out from what the character already knows, and a spell
        /// the multiworld has already handed over reads as known before the level is ever looked at.
        uint8 reqLevel;
        /// What this entry teaches when it is cast, for the few trainer entries that wrap a spell
        /// rather than being one -- a paladin's Judgement, the class mounts, Flight Form. The
        /// trainer casts those instead of teaching them, so the wrapper never passes the hook that
        /// keeps a randomized spell out of the character's hands: what comes out of it does.
        std::vector<uint32> taughtSpells;
    };

    /// The first ranks the seed took out of the trainers' hands, in both directions: the multiworld
    /// hands over item ids, while the game asks about spell ids.
    class Spells
    {
    public:
        Spells();

        void AddItem(int64_t itemId, uint32 spellId, uint8 reqLevel, std::vector<uint32> taughtSpells = {});
        /// Registers a spell the seed took out of the trainer's hands without an item of its own to
        /// hand it back. The riding ranks arrive that way: they share one progressive item between
        /// them, so they are known here by spell alone.
        void AddTrainerSpell(uint32 spellId, uint8 reqLevel);
        Optional<SpellItem> GetSpellByItemId(int64_t itemId) const;
        Optional<SpellItem> GetSpellBySpellId(uint32 spellId) const;
        bool IsRandomized(uint32 spellId) const;
        /// Whether the spell has an Archipelago item of its own, as opposed to only being something
        /// a wrapper would teach. A wrapper must not hand these out as a side effect.
        bool HasItemOfItsOwn(uint32 spellId) const;
        auto Begin() const { return bySpellId.cbegin(); }
        auto End() const { return bySpellId.cend(); }

    private:
        std::unordered_map<int64_t, SpellItem> byItemId;
        std::unordered_map<uint32, SpellItem> bySpellId;
        /// Spells with no entry of their own that a wrapper would otherwise teach for free
        std::unordered_set<uint32> taught;
    };
}

#endif
