#include "ArchipelaWoW.h"
#include "Define.h"
#include "Item.h"
#include "ItemScript.h"
#include "Player.h"
#include "Unit.h"

namespace ModArchipelaWoW::Scripts
{
    class ArchipelagoStoneScript : public ItemScript
    {
    public:
        ArchipelagoStoneScript() :
            ItemScript("item_archipelawow_ap_stone")
        {
        }

        bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override
        {
            if (!player || !item)
            {
                return false;
            }

            return sArchipelaWoW->OnUseArchipelagoStone(player, item);
        }

        void OnGossipSelect(Player* player, Item* item, uint32 sender, uint32 action) override
        {
            if (!player || !item)
            {
                return;
            }

            sArchipelaWoW->OnSelectArchipelagoStoneGossip(player, item, sender, action);
        }
    };

    void AddItemScripts()
    {
        new ArchipelagoStoneScript();
    }
}
