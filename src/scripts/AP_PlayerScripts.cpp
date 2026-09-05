#include "ArchipelaWoW.h"
#include "Channel.h"
#include "Creature.h"
#include "DBCStructure.h"
#include "Define.h"
#include "Item.h"
#include "NPCPackets.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "PlayerScript.h"
#include "QuestDef.h"
#include "scripts/AP_PlayerScripts.h"
#include "Trainer.h"
#include "Unit.h"

#include <string>

namespace ModArchipelaWoW::Scripts
{
    class AP_PlayerScript : public PlayerScript
    {
    public:
        AP_PlayerScript() :
            PlayerScript("ArchipelaWoW_PlayerScript", {
                PLAYERHOOK_ON_LOGIN,
                PLAYERHOOK_ON_BEFORE_LOGOUT,
                PLAYERHOOK_ON_ACHI_COMPLETE,
                PLAYERHOOK_ON_PLAYER_KILLED_BY_CREATURE,
                PLAYERHOOK_ON_PVP_KILL,
                PLAYERHOOK_ON_PLAYER_COMPLETE_QUEST,
                PLAYERHOOK_ON_DELETE,
                PLAYERHOOK_ON_GIVE_EXP,
                PLAYERHOOK_ON_BEFORE_GET_LEVEL_FOR_XP_GAIN,
                PLAYERHOOK_ON_LEARN_TAXI_NODE,
                PLAYERHOOK_ON_AFTER_TAKE_ITEM_FROM_MAIL,
                PLAYERHOOK_ON_CREATE_ITEM,
                PLAYERHOOK_CAN_LEARN_SPELL,
                PLAYERHOOK_ON_GET_TRAINER_SPELL_STATE,
                PLAYERHOOK_ON_BEFORE_RECEIVE_SPELL_LIST_FROM_TRAINER,
                PLAYERHOOK_ON_AFTER_TRAIN_SPELL,
                PLAYERHOOK_CAN_PLAYER_USE_CHAT,
                PLAYERHOOK_CAN_PLAYER_USE_CHANNEL_CHAT,
            })
        {
        }

        void OnPlayerLogin(Player* player) override
        {
            sArchipelaWoW->OnPlayerLogin(player);
        }

        void OnPlayerBeforeLogout(Player* player) override
        {
            sArchipelaWoW->OnPlayerBeforeLogout(player);
        }

        void OnPlayerAchievementComplete(Player* player, const AchievementEntry* achievement) override
        {
            sArchipelaWoW->OnPlayerAchievementComplete(player, achievement);
        }

        void OnPlayerKilledByCreature(Creature* killer, Player* killed) override
        {
            if (!killer || !killed)
            {
                return;
            }

            std::string cause = killed->GetName() + " was killed by " + killer->GetName();
            sArchipelaWoW->OnPlayerDied(killed, cause);
        }

        void OnPlayerPVPKill(Player* killer, Player* killed) override
        {
            if (!killer || !killed)
            {
                return;
            }

            std::string cause;
            if (killer->GetGUID() == killed->GetGUID())
            {
                cause = killed->GetName() + " died";
            }
            else
            {
                cause = killed->GetName() + " was killed by player " + killer->GetName();
            }

            sArchipelaWoW->OnPlayerDied(killed, cause);
        }

        void OnPlayerCompleteQuest(Player* player, const Quest* quest) override
        {
            if (!player || !quest)
            {
                return;
            }

            sArchipelaWoW->OnPlayerCompleteQuest(player, quest);
        }

        void OnPlayerDelete(ObjectGuid guid, uint32 /*account*/) override
        {
            sArchipelaWoW->OnPlayerDelete(guid);
        }

        void OnPlayerGiveXP(Player* player, uint32& amount, Unit* victim, uint8 xpSource) override
        {
            sArchipelaWoW->OnPlayerGiveXP(player, amount, victim, xpSource);
        }

        void OnPlayerBeforeGetLevelForXPGain(const Player* player, uint8& level) override
        {
            sArchipelaWoW->OnPlayerBeforeGetLevelForXPGain(player, level);
        }

        void OnPlayerLearnTaxiNode(const Player* player, uint32 node) override
        {
            sArchipelaWoW->OnPlayerLearnTaxiNode(player, node);
        }

        void OnPlayerAfterTakeItemFromMail(Player* player, Item* item, uint32 count) override
        {
            sArchipelaWoW->OnPlayerAfterTakeItemFromMail(player, item, count);
        }

        void OnPlayerCreateItem(Player* player, Item* item, uint32 count) override
        {
            sArchipelaWoW->OnPlayerCreateItem(player, item, count);
        }

        bool OnPlayerCanLearnSpell(Player* player, uint32 spellId) override
        {
            return sArchipelaWoW->OnPlayerCanLearnSpell(player, spellId);
        }

        void OnPlayerGetTrainerSpellState(const Player* player, uint32 /*trainerId*/, uint32 spellId, Trainer::SpellState& state) override
        {
            sArchipelaWoW->OnPlayerGetTrainerSpellState(player, spellId, state);
        }

        void OnPlayerBeforeReceiveSpellListFromTrainer(Player* player, Creature* /*creature*/, WorldPackets::NPC::TrainerList& trainerList) override
        {
            sArchipelaWoW->OnPlayerBeforeReceiveSpellListFromTrainer(player, trainerList);
        }

        void OnPlayerAfterTrainSpell(Player* player, Creature* creature, uint32 spellId) override
        {
            sArchipelaWoW->OnPlayerAfterTrainSpell(player, creature, spellId);
        }

        bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 /*language*/, std::string& msg) override
        {
            return sArchipelaWoW->OnPlayerCanUseChat(player, type, msg);
        }

        bool OnPlayerCanUseChat(Player* player, uint32 type, uint32 /*language*/, std::string& msg, Channel* channel) override
        {
            return sArchipelaWoW->OnPlayerCanUseChat(player, type, msg, channel ? channel->GetName() : "");
        }
    };

    void AddPlayerScripts()
    {
        new AP_PlayerScript();
    }
}
