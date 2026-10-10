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

#ifdef MOD_ARCHIPELAWOW_TRANSMOG
        void OnGossipSelectCode(Player* player, Item* /*item*/, uint32 sender, uint32 action, const char* code) override
        {
            if (!player)
            {
                return;
            }

            sArchipelaWoW->OnSelectArchipelagoStoneGossipCode(player, sender, action, code);
        }
#endif
    };

    class ArchipelagoConsumableScript : public ItemScript
    {
    public:
        ArchipelagoConsumableScript() :
            ItemScript("item_archipelawow_consumable")
        {
        }

        bool OnUse(Player* player, Item* item, SpellCastTargets const& /*targets*/) override
        {
            if (!player || !item)
            {
                return false;
            }

            // The item's spell, which uses it up, is left to be cast: the module applies the effect as
            // it is.
            if (sArchipelaWoW->CanUseArchipelagoConsumable(player, item))
            {
                return false;
            }

            // Releases the item the client greyed out on use, as the core's own item scripts do.
            player->SendEquipError(EQUIP_ERR_CANT_DO_RIGHT_NOW, item, nullptr);
            return true;
        }
    };

    void AddItemScripts()
    {
        new ArchipelagoStoneScript();
        new ArchipelagoConsumableScript();
    }
}
