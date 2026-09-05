#include "Define.h"
#include "items/AP_Spells.h"
#include "Optional.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace ModArchipelaWoW::Items
{
    Spells::Spells() :
        byItemId(),
        bySpellId(),
        taught()
    {
    }

    void Spells::AddItem(int64_t itemId, uint32 spellId, uint8 reqLevel, std::vector<uint32> taughtSpells)
    {
        for (uint32 taughtSpell : taughtSpells)
        {
            taught.insert(taughtSpell);
        }

        SpellItem item(itemId, spellId, reqLevel, std::move(taughtSpells));
        byItemId[itemId] = item;
        bySpellId[spellId] = std::move(item);
    }

    void Spells::AddTrainerSpell(uint32 spellId, uint8 reqLevel)
    {
        bySpellId[spellId] = SpellItem(0, spellId, reqLevel, {});
    }

    Optional<SpellItem> Spells::GetSpellByItemId(int64_t itemId) const
    {
        if (byItemId.contains(itemId))
        {
            return byItemId.at(itemId);
        }

        return {};
    }

    Optional<SpellItem> Spells::GetSpellBySpellId(uint32 spellId) const
    {
        if (bySpellId.contains(spellId))
        {
            return bySpellId.at(spellId);
        }

        return {};
    }

    bool Spells::IsRandomized(uint32 spellId) const
    {
        return bySpellId.contains(spellId) || taught.contains(spellId);
    }

    bool Spells::HasItemOfItsOwn(uint32 spellId) const
    {
        auto item = bySpellId.find(spellId);
        return item != bySpellId.end() && item->second.itemId != 0;
    }
}
