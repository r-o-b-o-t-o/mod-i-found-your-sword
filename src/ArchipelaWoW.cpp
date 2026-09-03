#include "AP_Character.h"
#include "AP_Config.h"
#include "ArchipelaWoW.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "DBCStructure.h"
#include "Define.h"
#include "Errors.h"
#include "fmt/core.h"
#include "IoContext.h"
#include "Item.h"
#include "items/AP_GearPool.h"
#include "network/AP_WebSocketService.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "Unit.h"

#include <memory>
#include <string>
#include <unordered_set>

#define ReturnIfModDisabled if (!config.IsEnabled()) { return; }

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

        std::string escapedName(name);
        WorldDatabase.EscapeString(escapedName);

        WorldDatabase.Query(
            "INSERT INTO `creature_template` (`entry`, `name`, `faction`, `unit_class`) VALUES ({}, '{}', 18, 1)",
            nextEntry, escapedName
        );
        WorldDatabase.Execute(
            "INSERT INTO `ap_player_creature_template` (`player`, `creatureEntry`) VALUES ('{}', {})",
            escapedName, nextEntry
        );
        sObjectMgr->LoadCreatureTemplates();

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

    const Config& ArchipelaWoW::GetConfig()
    {
        return config;
    }

    void ArchipelaWoW::InitializeConfig(bool reload)
    {
        config.Initialize(reload);
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

        QueryResult result = WorldDatabase.Query("SELECT `player`, `creatureEntry` FROM `ap_player_creature_template`");
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
        ReturnIfModDisabled;

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

    void ArchipelaWoW::OnPlayerDelete(ObjectGuid playerGuid)
    {
        ReturnIfModDisabled;

        auto guid = playerGuid.GetCounter();
        if (apCharacters.contains(guid))
        {
            delete apCharacters[guid];
            apCharacters.erase(guid);
        }
        CharacterDatabase.Execute("DELETE FROM `ap_character` WHERE `guid` = {}", guid);
        CharacterDatabase.Execute("DELETE FROM `ap_location_check` WHERE `guid` = {}", guid);
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

        if (AP_Character::IsSlotBound(slot))
        {
            ChatHandler(player->GetSession()).SendSysMessage(fmt::format("|cFFFF0000Archipelago slot \"{}\" is already bound to an existing character.", slot));
            return true;
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

    void ArchipelaWoW::OnNetworkStart(Acore::Asio::IoContext& ioContext)
    {
        wsService = std::make_unique<Network::WebSocketService>(ioContext.get_executor());
    }
}
