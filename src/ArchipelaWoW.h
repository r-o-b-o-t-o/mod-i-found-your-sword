#ifndef _MOD_ARCHIPELAWOW_ARCHIPELAWOW_H_
#define _MOD_ARCHIPELAWOW_ARCHIPELAWOW_H_

#include "AP_Config.h"
#include "DBCStructure.h"
#include "Define.h"
#include "IoContext.h"
#include "Item.h"
#include "network/AP_WebSocketService.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "QuestDef.h"
#include "Unit.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace ModArchipelaWoW
{
    class AP_Character;

    class ArchipelaWoW
    {
    public:
        ArchipelaWoW();
        static ArchipelaWoW* Instance();

        uint32 GetCreatureTemplateForPlayer(std::string name);
        Network::WebSocketService& GetWebSocketService();

        // Config methods
        const Config& GetConfig();

        // WorldScripts methods
        void OnBeforeConfigLoad(bool reload);
        void OnWorldUpdate();
        void OnShutdown();

        // PlayerScripts methods
        void OnPlayerLogin(Player* player);
        void OnPlayerBeforeLogout(Player* player);
        void OnPlayerAchievementComplete(Player* player, const AchievementEntry* achievement);
        void OnPlayerCompleteQuest(Player* player, const Quest* quest);
        void OnPlayerDied(Player* player, const std::string& cause);
        void OnPlayerDelete(ObjectGuid playerGuid);
        void OnPlayerGiveXP(Player* player, uint32& amount, Unit* victim, uint8 xpSource);
        void OnPlayerBeforeGetLevelForXPGain(const Player* player, uint8& level);
        void OnPlayerLearnTaxiNode(const Player* player, uint32 nodeId);

        // CommandScripts methods
        bool HandleAPConnectCommand(Player* player, std::string slot);

        // ItemScripts methods
        bool OnUseArchipelagoStone(Player* player, Item* item);
        void OnSelectArchipelagoStoneGossip(Player* player, Item* item, uint32 sender, uint32 action);

        // ServerScripts methods
        void OnNetworkStart(Acore::Asio::IoContext& ioContext);

    private:
        Config config;
        std::unordered_map<ObjectGuid::LowType, AP_Character*> apCharacters;
        std::unordered_map<std::string, uint32> playerCreatureTemplates;
        std::unique_ptr<Network::WebSocketService> wsService;

        void InitializeConfig(bool reload);
        void LoadPlayerCreatureTemplates();
    };
}

#define sArchipelaWoW ModArchipelaWoW::ArchipelaWoW::Instance()

#endif
