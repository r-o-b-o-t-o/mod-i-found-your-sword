#include "AP_Character.h"
#include "AP_Config.h"
#include "AP_Log.h"
#include "ArchipelaWoW.h"
#include "Chat.h"
#include "Creature.h"
#include "database/AP_Database.h"
#include "database/AP_DatabaseConnection.h"
#include "DatabaseEnv.h"
#include "DatabaseEnvFwd.h"
#include "DBCStructure.h"
#include "Define.h"
#include "Errors.h"
#include "fmt/format.h"
#include "InstanceScript.h"
#include "IoContext.h"
#include "Item.h"
#include "items/AP_GearPool.h"
#include "locations/AP_Bosses.h"
#include "Map.h"
#include "MapReference.h"
#include "network/AP_WebSocketService.h"
#include "NPCPackets.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Opcodes.h"
#include "Optional.h"
#include "Player.h"
#include "QuestDef.h"
#include "Spell.h"
#include "Trainer.h"
#include "Unit.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_set>

#define ReturnIfModDisabled if (!config.IsEnabled()) { return; }
#define ReturnValueIfModDisabled(value) if (!config.IsEnabled()) { return value; }

namespace ModArchipelaWoW
{
    ArchipelaWoW::ArchipelaWoW() :
        config(),
        apCharacters(),
        playerCreatureTemplates(),
        gearPool(),
        gearPoolBuilt(false)
    {
    }

    ArchipelaWoW* ArchipelaWoW::Instance()
    {
        static ArchipelaWoW instance;
        return &instance;
    }

    uint32 ArchipelaWoW::GetCreatureTemplateForPlayer(std::string name)
    {
        if (playerCreatureTemplates.contains(name))
        {
            return playerCreatureTemplates[name];
        }

        uint32 nextEntry = 5150000;
        for (const auto& [_, entry] : playerCreatureTemplates)
        {
            if (entry >= nextEntry)
            {
                nextEntry = entry + 1;
            }
        }

        // A core table, so no prepared statement to reach for; synchronous because the load
        // right below has to see the row.
        std::string escapedName(name);
        WorldDatabase.EscapeString(escapedName);
        WorldDatabase.DirectExecute(
            "INSERT INTO `creature_template` (`entry`, `name`, `faction`, `unit_class`) VALUES ({}, '{}', 18, 1)",
            nextEntry, escapedName
        );

        Database::AP_DatabasePreparedStatement* stmt = Database::ArchipelaWoWDatabase.GetPreparedStatement(Database::AP_INS_PLAYER_CREATURE_TEMPLATE);
        stmt->SetArguments(name, nextEntry);
        Database::ArchipelaWoWDatabase.Execute(stmt);

        // Only the new entry, the way `.reload creature_template` does it: reloading every template would
        // wipe the flags the core sets on them at startup, such as the dungeon boss flag that encounter
        // credit depends on.
        WorldDatabasePreparedStatement* templateStmt = WorldDatabase.GetPreparedStatement(WORLD_SEL_CREATURE_TEMPLATE);
        templateStmt->SetData(0, nextEntry);
        if (PreparedQueryResult result = WorldDatabase.Query(templateStmt))
        {
            sObjectMgr->LoadCreatureTemplate(result->Fetch(), true);
            sObjectMgr->CheckCreatureTemplate(sObjectMgr->GetCreatureTemplate(nextEntry));
        }

        playerCreatureTemplates[name] = nextEntry;

        return nextEntry;
    }

    Network::WebSocketService& ArchipelaWoW::GetWebSocketService()
    {
        ASSERT_NOTNULL(wsService.get());
        return *wsService;
    }

    const Items::GearPool& ArchipelaWoW::GetGearPool() const
    {
        return gearPool;
    }

    const std::unordered_map<ObjectGuid::LowType, AP_Character*>& ArchipelaWoW::GetCharacters() const
    {
        return apCharacters;
    }

    const Config& ArchipelaWoW::GetConfig()
    {
        return config;
    }

    void ArchipelaWoW::InitializeConfig(bool reload)
    {
        // First built in OnModuleDatabasesLoading; a second build trips the cache's overwrite guard.
        if (reload)
        {
            config.Initialize(true);
        }

        LoadPlayerCreatureTemplates();

        // A reload is how the module gets switched on without restarting worldserver, and OnStartup
        // is long past by then. Item templates are loaded well before any reload, so the pool can be
        // built from here too -- without this, enabling the module that way leaves every gear reward
        // with nothing to draw from.
        if (reload)
        {
            BuildGearPool();
        }
    }

    void ArchipelaWoW::BuildGearPool()
    {
        ReturnIfModDisabled;

        if (gearPoolBuilt)
        {
            return;
        }

        gearPool.Build();
        gearPoolBuilt = true;
    }

    void ArchipelaWoW::LoadPlayerCreatureTemplates()
    {
        playerCreatureTemplates.clear();
        ReturnIfModDisabled;

        Database::AP_DatabasePreparedStatement* stmt = Database::ArchipelaWoWDatabase.GetPreparedStatement(Database::AP_SEL_PLAYER_CREATURE_TEMPLATES);
        PreparedQueryResult result = Database::ArchipelaWoWDatabase.Query(stmt);
        if (!result)
        {
            return;
        }

        do
        {
            std::string name = (*result)[0].Get<std::string>();
            uint32 entry = (*result)[1].Get<uint32>();
            playerCreatureTemplates[name] = entry;
        } while (result->NextRow());
    }

    bool ArchipelaWoW::OnModuleDatabasesLoading()
    {
        // The config file is read by now and the pool needs its options, so the cache is built
        // here rather than in OnBeforeConfigLoad, which then only ever reloads it.
        config.Initialize(false);
        return Database::ArchipelaWoWDatabase.Load(config);
    }

    void ArchipelaWoW::OnBeforeConfigLoad(bool reload)
    {
        InitializeConfig(reload);
    }

    void ArchipelaWoW::OnStartup()
    {
        // Deliberately here rather than in OnBeforeConfigLoad: the pool reads the item template
        // store, which worldserver only fills once the world is done initializing.
        BuildGearPool();
    }

    void ArchipelaWoW::OnWorldUpdate()
    {
        ReturnIfModDisabled;

        std::unordered_set<ObjectGuid::LowType> apCharsToDelete;

        for (auto& [key, apCharacter] : apCharacters)
        {
            bool run = apCharacter->Update();
            if (!run)
            {
                apCharsToDelete.insert(key);
            }
        }

        for (auto& key : apCharsToDelete)
        {
            delete apCharacters[key];
            apCharacters.erase(key);
        }
    }

    void ArchipelaWoW::OnShutdown()
    {
        for (auto& [key, apCharacter] : apCharacters)
        {
            apCharacter->OnPlayerBeforeLogout();
            delete apCharacter;
        }
        apCharacters.clear();

        if (wsService)
        {
            wsService.reset();
        }
    }

    void ArchipelaWoW::OnPlayerLogin(Player* player)
    {
        ReturnIfModDisabled;

        if (!player || !player->GetSession())
        {
            return;
        }

        if (config.ShouldAnnounce())
        {
            ChatHandler(player->GetSession()).SendSysMessage("This server is running the |cFF4CFF00ArchipelaWoW|r module.");
        }

        AP_Character* apCharacter = AP_Character::LoadFromDatabase(player);
        if (apCharacter)
        {
            apCharacters[player->GetGUID().GetCounter()] = apCharacter;
        }
        else
        {
            ChatHandler(player->GetSession()).SendSysMessage("Type |cFF5F5FD7.ap connect SlotName|r to connect to the Archipelago multiworld.");
        }
    }

    void ArchipelaWoW::OnPlayerBeforeLogout(Player* player)
    {
        if (!player)
        {
            return;
        }

        auto guid = player->GetGUID().GetCounter();
        if (apCharacters.contains(guid))
        {
            apCharacters[guid]->OnPlayerBeforeLogout();
            delete apCharacters[guid];
            apCharacters.erase(guid);
        }
    }

    void ArchipelaWoW::OnPlayerAchievementComplete(Player* player, const AchievementEntry* achievement)
    {
        ReturnIfModDisabled;

        if (!player)
        {
            return;
        }

        auto guid = player->GetGUID().GetCounter();
        if (apCharacters.contains(guid))
        {
            apCharacters[guid]->OnPlayerAchievementComplete(achievement);
        }
    }

    void ArchipelaWoW::OnPlayerCriteriaProgress(Player* player, const AchievementCriteriaEntry* criteria)
    {
        ReturnIfModDisabled;

        if (!player)
        {
            return;
        }

        auto guid = player->GetGUID().GetCounter();
        if (apCharacters.contains(guid))
        {
            apCharacters[guid]->OnPlayerCriteriaProgress(criteria);
        }
    }

    void ArchipelaWoW::OnPlayerCompleteQuest(Player* player, const Quest* quest)
    {
        ReturnIfModDisabled;

        if (!player || !quest)
        {
            return;
        }

        auto guid = player->GetGUID().GetCounter();
        if (apCharacters.contains(guid))
        {
            apCharacters[guid]->OnPlayerCompleteQuest(quest);
        }
    }

    void ArchipelaWoW::OnPlayerDied(Player* player, const std::string& cause)
    {
        ReturnIfModDisabled;

        if (!player)
        {
            return;
        }

        auto guid = player->GetGUID().GetCounter();
        if (apCharacters.contains(guid))
        {
            apCharacters[guid]->OnPlayerDied(cause);
        }
    }

    void ArchipelaWoW::OnPlayerDeleteFromDB(ObjectGuid::LowType guid)
    {
        // OnPlayerDelete only covers the client's delete button -- `.character erase`, account
        // deletion and the old-character purge bypass it -- and fires even when CharDelete.Method
        // merely unlinks a restorable character. This hook runs at the real removal on every path,
        // and must run with the module disabled too: the guid is gone for good afterwards.
        ReleaseBinding(guid);
    }

    void ArchipelaWoW::ReleaseBinding(ObjectGuid::LowType guid)
    {
        // Freed without a save, which would write the rows straight back: the character can still be
        // online, as on `.character erase`, which removes the rows a tick before logging it out.
        if (apCharacters.contains(guid))
        {
            delete apCharacters[guid];
            apCharacters.erase(guid);
        }

        Database::AP_DatabaseTransaction trans = Database::ArchipelaWoWDatabase.BeginTransaction();

        Database::AP_DatabasePreparedStatement* stmt = Database::ArchipelaWoWDatabase.GetPreparedStatement(Database::AP_DEL_CHARACTER);
        stmt->SetData(0, guid);
        trans->Append(stmt);

        stmt = Database::ArchipelaWoWDatabase.GetPreparedStatement(Database::AP_DEL_LOCATION_CHECKS);
        stmt->SetData(0, guid);
        trans->Append(stmt);

        stmt = Database::ArchipelaWoWDatabase.GetPreparedStatement(Database::AP_DEL_PREFERENCES);
        stmt->SetData(0, guid);
        trans->Append(stmt);

        Database::ArchipelaWoWDatabase.CommitTransaction(trans);
    }

    void ArchipelaWoW::OnPlayerGiveXP(Player* player, uint32& amount, Unit* victim, uint8 xpSource)
    {
        ReturnIfModDisabled;

        if (!player)
        {
            return;
        }

        auto guid = player->GetGUID().GetCounter();
        if (apCharacters.contains(guid))
        {
            apCharacters[guid]->OnPlayerGiveXP(amount, victim, xpSource);
        }
    }

    void ArchipelaWoW::OnPlayerBeforeGetLevelForXPGain(const Player* player, uint8& level)
    {
        ReturnIfModDisabled;

        if (!player)
        {
            return;
        }

        auto guid = player->GetGUID().GetCounter();
        if (apCharacters.contains(guid))
        {
            apCharacters[guid]->OnPlayerBeforeGetLevelForXPGain(level);
        }
    }

    void ArchipelaWoW::OnPlayerLevelChanged(Player* player)
    {
        ReturnIfModDisabled;

        if (!player)
        {
            return;
        }

        auto guid = player->GetGUID().GetCounter();
        if (apCharacters.contains(guid))
        {
            apCharacters[guid]->OnPlayerLevelChanged();
        }
    }

    void ArchipelaWoW::OnPlayerSendInitialPacketsBeforeAddToMap(Player* player)
    {
        ReturnIfModDisabled;

        if (!player)
        {
            return;
        }

        auto guid = player->GetGUID().GetCounter();
        if (apCharacters.contains(guid))
        {
            apCharacters[guid]->OnPlayerSendInitialPacketsBeforeAddToMap();
        }
    }

    void ArchipelaWoW::OnPlayerLearnTaxiNode(const Player* player, uint32 nodeId)
    {
        ReturnIfModDisabled;

        if (!player)
        {
            return;
        }

        auto guid = player->GetGUID().GetCounter();
        if (apCharacters.contains(guid))
        {
            apCharacters[guid]->OnPlayerLearnTaxiNode(nodeId);
        }
    }

    void ArchipelaWoW::OnPlayerTextEmote(Player* player, uint32 textEmote, ObjectGuid target)
    {
        ReturnIfModDisabled;

        if (!player)
        {
            return;
        }

        auto guid = player->GetGUID().GetCounter();
        if (apCharacters.contains(guid))
        {
            apCharacters[guid]->OnPlayerTextEmote(textEmote, target);
        }
    }

    void ArchipelaWoW::OnPlayerAfterTakeItemFromMail(Player* player, Item* item, uint32 /*count*/)
    {
        ReturnIfModDisabled;

        if (!player || !item)
        {
            return;
        }

        auto guid = player->GetGUID().GetCounter();
        if (apCharacters.contains(guid))
        {
            // Only the entry travels onwards: which tier was collected is all the character needs,
            // and nothing downstream then holds a raw Item* it would have to reason about.
            apCharacters[guid]->OnPlayerAfterTakeItemFromMail(item->GetEntry());
        }
    }

    bool ArchipelaWoW::OnPlayerCanLearnSpell(Player* player, uint32 spellId)
    {
        ReturnValueIfModDisabled(true);

        if (!player)
        {
            return true;
        }

        auto guid = player->GetGUID().GetCounter();
        if (!apCharacters.contains(guid))
        {
            return true;
        }

        return apCharacters[guid]->OnPlayerCanLearnSpell(spellId);
    }

    void ArchipelaWoW::OnPlayerGetTrainerSpellState(const Player* player, uint32 spellId, Trainer::SpellState& state)
    {
        ReturnIfModDisabled;

        if (!player)
        {
            return;
        }

        auto guid = player->GetGUID().GetCounter();
        if (apCharacters.contains(guid))
        {
            apCharacters[guid]->OnPlayerGetTrainerSpellState(spellId, state);
        }
    }

    void ArchipelaWoW::OnPlayerBeforeReceiveSpellListFromTrainer(Player* player, WorldPackets::NPC::TrainerList& trainerList)
    {
        ReturnIfModDisabled;

        if (!player)
        {
            return;
        }

        auto guid = player->GetGUID().GetCounter();
        if (apCharacters.contains(guid))
        {
            apCharacters[guid]->OnPlayerBeforeReceiveSpellListFromTrainer(trainerList);
        }
    }

    void ArchipelaWoW::OnPlayerAfterTrainSpell(Player* player, Creature* trainer, uint32 spellId)
    {
        ReturnIfModDisabled;

        if (!player)
        {
            return;
        }

        auto guid = player->GetGUID().GetCounter();
        if (apCharacters.contains(guid))
        {
            apCharacters[guid]->OnPlayerAfterTrainSpell(trainer, spellId);
        }
    }

    void ArchipelaWoW::OnPlayerUpdateSkill(Player* player, uint32 skillId, uint32 newValue)
    {
        ReturnIfModDisabled;

        if (!player)
        {
            return;
        }

        auto guid = player->GetGUID().GetCounter();
        if (apCharacters.contains(guid))
        {
            apCharacters[guid]->OnPlayerUpdateSkill(skillId, newValue);
        }
    }

    void ArchipelaWoW::OnPlayerCreateItem(Player* player, Item* item, uint32 /*count*/)
    {
        ReturnIfModDisabled;

        if (!player || !item)
        {
            return;
        }

        auto guid = player->GetGUID().GetCounter();
        if (apCharacters.contains(guid))
        {
            apCharacters[guid]->OnPlayerCreateItem(item);
        }
    }

    bool ArchipelaWoW::OnPlayerCanUseChat(Player* player, uint32 type, const std::string& msg, const std::string& channelName)
    {
        ReturnValueIfModDisabled(true);

        if (!player)
        {
            return true;
        }

        auto guid = player->GetGUID().GetCounter();
        if (!apCharacters.contains(guid))
        {
            return true;
        }

        return apCharacters[guid]->OnPlayerChat(type, msg, channelName);
    }

    void ArchipelaWoW::OnAfterUpdateEncounterState(Map* map, EncounterCreditType type, uint32 creditEntry, const DungeonEncounterList* encounters)
    {
        ReturnIfModDisabled;

        if (!map || !encounters)
        {
            return;
        }

        for (const DungeonEncounter* encounter : *encounters)
        {
            if (encounter->creditType == type && encounter->creditEntry == creditEntry)
            {
                CreditDungeonEncounter(map, encounter->dbcEntry->id);
            }
        }
    }

    void ArchipelaWoW::OnInitializeLockedDungeons(Player* player, uint32& lockData, const lfg::LFGDungeonData* dungeon)
    {
        ReturnIfModDisabled;

        if (!player || !dungeon)
        {
            return;
        }

        auto guid = player->GetGUID().GetCounter();
        if (apCharacters.contains(guid))
        {
            apCharacters[guid]->OnInitializeLockedDungeons(lockData, dungeon);
        }
    }

    void ArchipelaWoW::OnPlayerCreatureKillCredit(Player* player, Creature* killed)
    {
        ReturnIfModDisabled;

        if (!player || !killed)
        {
            return;
        }

        // Called once per kill, for the player credited with it, so a pet's or a group member's kill counts too
        for (const Locations::SyntheticEncounter& encounter : Locations::SYNTHETIC_ENCOUNTERS)
        {
            if (killed->GetEntry() == encounter.creatureEntry && killed->GetMapId() == encounter.mapId)
            {
                CreditDungeonEncounter(killed->GetMap(), encounter.encounterId);
            }
        }

        const auto& defenders = Locations::ATALAI_DEFENDERS;
        if (killed->GetMapId() == Locations::SUNKEN_TEMPLE_MAP_ID && std::find(defenders.begin(), defenders.end(), killed->GetEntry()) != defenders.end())
        {
            // The defender's SmartAI has already counted this death, its death event runs before this hook
            InstanceScript* instance = killed->GetInstanceScript();
            if (instance && instance->GetData(Locations::SUNKEN_TEMPLE_DATA_DEFENDER_KILLED) == defenders.size())
            {
                CreditDungeonEncounter(killed->GetMap(), Locations::ATALAI_DEFENDERS_ENCOUNTER_ID);
            }
        }
    }

    void ArchipelaWoW::CreditDungeonEncounter(Map* map, uint32 encounterId)
    {
        // Everyone in the instance, the way the core marks the encounter done for the whole instance.
        for (const MapReference& ref : map->GetPlayers())
        {
            Player* player = ref.GetSource();
            if (!player)
            {
                continue;
            }

            auto guid = player->GetGUID().GetCounter();
            if (apCharacters.contains(guid))
            {
                apCharacters[guid]->OnDungeonEncounterCredited(encounterId);
            }
        }
    }

    bool ArchipelaWoW::HandleAPConnectCommand(Player* player, std::string slot)
    {
        if (!config.IsEnabled() || !player || !player->GetSession())
        {
            return true;
        }

        auto guid = player->GetGUID().GetCounter();
        if (apCharacters.contains(guid))
        {
            ChatHandler(player->GetSession()).SendSysMessage("|cFFFF0000This character is already bound to an Archipelago slot.");
            return true;
        }

        if (Optional<AP_Character::SlotBinding> binding = AP_Character::GetSlotBinding(slot))
        {
            if (!binding->goalCompleted)
            {
                ChatHandler(player->GetSession()).SendSysMessage(fmt::format("|cFFFF0000Archipelago slot \"{}\" is already bound to an existing character.", slot));
                return true;
            }

            // A finished seed frees its slot name for the next one. The old character is unbound rather
            // than left as is: on its next login, it would connect to the new seed's slot and could send
            // it its checks and goal.
            if (apCharacters.contains(binding->guid))
            {
                ChatHandler(apCharacters[binding->guid]->GetPlayer()->GetSession()).SendSysMessage(fmt::format("|cFFFF0000Archipelago slot |cFFFF00FF{}|cFFFF0000 was taken over by another character, this character is no longer bound to it.", slot));
            }

            AP_LOG_INFO("Releasing finished Archipelago slot {} from character {} for character {}.", slot, binding->guid, guid);
            ReleaseBinding(binding->guid);
        }

        apCharacters[guid] = new AP_Character(player, slot);
        return true;
    }

    bool ArchipelaWoW::HandleAPLevelCommand(Player* player)
    {
        if (!config.IsEnabled() || !player || !player->GetSession())
        {
            return true;
        }

        auto guid = player->GetGUID().GetCounter();
        if (!apCharacters.contains(guid))
        {
            ChatHandler(player->GetSession()).SendSysMessage("|cFFFF0000This character is not bound to an Archipelago slot. Use |cFF5F5FD7.ap connect SlotName|cFFFF0000 first.");
            return true;
        }

        apCharacters[guid]->SendLevelReport();
        return true;
    }

    bool ArchipelaWoW::HandleAPSayCommand(Player* player, const std::string& text)
    {
        if (!config.IsEnabled() || !player || !player->GetSession())
        {
            return true;
        }

        auto guid = player->GetGUID().GetCounter();
        if (!apCharacters.contains(guid))
        {
            ChatHandler(player->GetSession()).SendSysMessage("|cFFFF0000This character is not bound to an Archipelago slot. Use |cFF5F5FD7.ap connect SlotName|cFFFF0000 first.");
            return true;
        }

        apCharacters[guid]->SayToArchipelago(text);
        return true;
    }

    bool ArchipelaWoW::OnUseArchipelagoStone(Player* player, Item* item)
    {
        if (player && item)
        {
            auto guid = player->GetGUID().GetCounter();
            if (apCharacters.contains(guid))
            {
                apCharacters[guid]->OnUseArchipelagoStone(item);
            }
        }

        return false;
    }

    void ArchipelaWoW::OnPlayerSpellCast(Player* player, const Spell* spell)
    {
        // Every spell a character casts comes through here: bail out before the lookup.
        if (!player || !spell || !spell->m_CastItem)
        {
            return;
        }

        auto guid = player->GetGUID().GetCounter();
        if (apCharacters.contains(guid))
        {
            apCharacters[guid]->OnPlayerSpellCast(spell);
        }
    }

    void ArchipelaWoW::OnPlayerResurrect(Player* player)
    {
        if (!player)
        {
            return;
        }

        auto guid = player->GetGUID().GetCounter();
        if (apCharacters.contains(guid))
        {
            apCharacters[guid]->OnPlayerResurrect();
        }
    }

    bool ArchipelaWoW::CanUseArchipelagoConsumable(Player* player, Item* item)
    {
        auto guid = player->GetGUID().GetCounter();
        if (!apCharacters.contains(guid))
        {
            ChatHandler(player->GetSession()).SendSysMessage("|cFFFF0000This character is not bound to an Archipelago slot.");
            return false;
        }

        return apCharacters[guid]->CanUseConsumable(item->GetEntry());
    }

    void ArchipelaWoW::OnSelectArchipelagoStoneGossip(Player* player, Item* item, uint32 sender, uint32 action)
    {
        if (!player || !item)
        {
            return;
        }

        auto guid = player->GetGUID().GetCounter();
        if (apCharacters.contains(guid))
        {
            apCharacters[guid]->OnSelectArchipelagoStoneGossip(item, sender, action);
        }
    }

#ifdef MOD_ARCHIPELAWOW_TRANSMOG
    void ArchipelaWoW::OnSelectArchipelagoStoneGossipCode(Player* player, uint32 sender, uint32 action, const char* code)
    {
        if (!player)
        {
            return;
        }

        auto guid = player->GetGUID().GetCounter();
        if (apCharacters.contains(guid))
        {
            apCharacters[guid]->OnSelectArchipelagoStoneGossipCode(sender, action, code);
        }
    }
#endif

    void ArchipelaWoW::OnNetworkStart(Acore::Asio::IoContext& ioContext)
    {
        wsService = std::make_unique<Network::WebSocketService>(ioContext.get_executor());
    }

#ifdef MOD_ARCHIPELAWOW_TRANSMOG
    bool ArchipelaWoW::CanPacketSend(WorldSession* session, const WorldPacket& packet)
    {
        // Nearly every packet sent to a client comes through here: bail out before the lookup.
        uint16 opcode = packet.GetOpcode();
        if ((opcode != SMSG_GOSSIP_MESSAGE && opcode != SMSG_LIST_INVENTORY) || !session || !session->GetPlayer())
        {
            return true;
        }

        auto guid = session->GetPlayer()->GetGUID().GetCounter();
        if (!apCharacters.contains(guid))
        {
            return true;
        }

        return apCharacters[guid]->CanPacketSend(packet);
    }
#endif

    bool ArchipelaWoW::CanPacketReceive(WorldSession* session, const WorldPacket& packet)
    {
        // Nearly every packet received from a client comes through here: bail out before the lookup.
        switch (packet.GetOpcode())
        {
            case CMSG_ITEM_QUERY_SINGLE:
            case CMSG_BUY_ITEM:
            case CMSG_BUY_ITEM_IN_SLOT:
            case CMSG_GET_MAIL_LIST:
            case CMSG_SEND_MAIL:
            case CMSG_MAIL_TAKE_MONEY:
            case CMSG_MAIL_TAKE_ITEM:
            case CMSG_MAIL_MARK_AS_READ:
            case CMSG_MAIL_RETURN_TO_SENDER:
            case CMSG_MAIL_DELETE:
            case CMSG_MAIL_CREATE_TEXT_ITEM:
            case CMSG_ACTIVATETAXI:
            case CMSG_ACTIVATETAXIEXPRESS:
                break;
            default:
                return true;
        }

        if (!session || !session->GetPlayer())
        {
            return true;
        }

        auto character = apCharacters.find(session->GetPlayer()->GetGUID().GetCounter());
        if (character == apCharacters.end())
        {
            return true;
        }

        return character->second->CanPacketReceive(packet);
    }
}
