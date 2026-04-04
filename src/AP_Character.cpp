#include "AP_Character.h"
#include "AP_PlayerPosition.h"
#include "ArchipelaWoW.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "Define.h"
#include "Errors.h"
#include "Field.h"
#include "fmt/core.h"
#include "GameTime.h"
#include "Item.h"
#include "items/AP_ItemsContainer.h"
#include "items/AP_Zones.h"
#include "ItemTemplate.h"
#include "Mail.h"
#include "nlohmann/json.hpp"
#include "ObjectMgr.h"
#include "Player.h"
#include "QuestDef.h"
#include "Unit.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <list>
#include <string>
#include <unordered_map>

#define sConfig sArchipelaWoW->GetConfig()
constexpr auto AP_GAME_NAME = "World of Warcraft";
constexpr uint32 TRESPASSER_SPELL_ID = 73536; // Trespasser - spell with teleport visual effect and short stun, used when player tries to enter a locked zone
constexpr uint32 TELEPORT_SPELL_ID = 7141; // Simple Teleport - visual effect, used when teleporting with the Archipelago Stone

namespace ModArchipelaWoW
{
    AP_Character::AP_Character(Player* player, std::string slot) :
        player(player),
        slot(slot),
        itemIndex(-1),
        lastDeathTime(-1.0),
        deathLinkEnabled(false),
        itemsSynced(false),
        run(true),
        tags(),
        items(),
        locations(),
        apStone(this),
        unlockedZones(),
        lastUnlockedPosition(player->GetMapId(), player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetOrientation()),
        nextLockedZoneCheck(GameTime::GetGameTime() + std::chrono::seconds(6)),
        nextSave(GameTime::GetGameTime() + std::chrono::seconds(15)),
        goalAchievementId(0),
        maxLevel(0),
        apLevel(1),
        apExp(0),
        xpForLevel(0),
        goalCompleted(false)
    {
        ASSERT_NOTNULL(player);
        ASSERT_NOTNULL(player->GetSession());

        uuid = GenerateUUID();
        LoadXPForLevel();
    }

    AP_Character::AP_Character(Player* player, std::string uuid, std::string slot, int itemIndex, uint8 apLevel, uint32 apExp, bool goalCompleted) :
        AP_Character(player, slot)
    {
        this->uuid = uuid;
        this->itemIndex = itemIndex;
        this->apLevel = apLevel;
        this->apExp = apExp;
        this->goalCompleted = goalCompleted;

        LoadXPForLevel();
    }

    AP_Character::~AP_Character()
    {
        if (ap)
        {
            ap->Reset();
        }
    }

    bool AP_Character::Update()
    {
        if (run)
        {
            if (!ap)
            {
                CreateAPClient();
            }

            if (GameTime::GetGameTime() >= nextSave)
            {
                SaveToDatabase();
            }

            CheckIsInLockedZone();
            SavePosition();
            ap->Poll();
        }

        return run;
    }

    const Items::ItemsContainer& AP_Character::GetItemsContainer() const
    {
        return items;
    }

    Player* AP_Character::GetPlayer()
    {
        return player;
    }

    std::string AP_Character::GetItemName(int64 itemId) const
    {
        if (!ap)
        {
            return "";
        }
        return ap->GetItemName(itemId, AP_GAME_NAME);
    }

    bool AP_Character::IsZoneUnlocked(uint32 zoneId) const
    {
        return unlockedZones.contains(zoneId);
    }

    void AP_Character::Teleport(const Items::ZoneItem& zone)
    {
        if (IsZoneUnlocked(zone.id))
        {
            player->TeleportTo(zone.map, zone.x, zone.y, zone.z, zone.o);
            player->CastSpell(player, TELEPORT_SPELL_ID, true);
        }
    }

    void AP_Character::OnPlayerAchievementComplete(const AchievementEntry* achievement)
    {
        if (!achievement)
        {
            return;
        }

        auto checkId = locations.achievements.GetLocationId(achievement);
        if (checkId.has_value())
        {
            CheckLocation(checkId.value());
        }

        if (achievement->ID == goalAchievementId)
        {
            goalCompleted = true;
            SaveToDatabase();
            if (ap)
            {
                ap->StatusUpdate(Network::Client::ClientStatus::Goal);
            }
        }
    }

    void AP_Character::OnPlayerDied(const std::string& cause)
    {
        if (!ap || !deathLinkEnabled)
        {
            return;
        }

        double timestamp = ap->GetServerTime();
        if (lastDeathTime > 0.0 && timestamp - 3.0 < lastDeathTime)
        {
            // Prevent from sending a death link if we just died from one
            return;
        }

        nlohmann::json data
        {
            { "cause", cause },
            { "source", slot },
            { "time", timestamp },
        };
        lastDeathTime = timestamp;
        ap->Bounce(data, {}, {}, { "DeathLink" });
    }

    void AP_Character::OnPlayerCompleteQuest(const Quest* quest)
    {
        if (!quest)
        {
            return;
        }

        auto questId = quest->GetQuestId();
        auto checkId = locations.quests.GetLocationId(quest);
        if (checkId.has_value())
        {
            CheckLocation(checkId.value());
        }
    }

    void AP_Character::OnPlayerGiveXP(uint32& xp, Unit* victim, uint8 xpSource)
    {
        if (apLevel >= maxLevel)
        {
            xp = 0;
            return;
        }

        apExp += xp;

        while (xpForLevel > 0 && apExp >= xpForLevel)
        {
            ++apLevel;
            apExp -= xpForLevel;

            auto checkId = locations.levels.GetLocationId(apLevel);
            if (checkId.has_value())
            {
                CheckLocation(checkId.value());
            }

            LoadXPForLevel();
        }

        xp = 0;
    }

    void AP_Character::OnPlayerBeforeGetLevelForXPGain(uint8& level)
    {
        level = apLevel;
    }

    void AP_Character::OnPlayerLearnTaxiNode(uint32 nodeId)
    {
        auto checkId = locations.flightPaths.GetLocationId(nodeId);
        if (checkId.has_value())
        {
            CheckLocation(checkId.value());
        }
    }

    void AP_Character::OnPlayerBeforeLogout()
    {
        SaveToDatabase();
    }

    void AP_Character::OnUseArchipelagoStone(Item* item)
    {
        if (!item)
        {
            return;
        }

        apStone.OnUse(item);
    }

    void AP_Character::OnSelectArchipelagoStoneGossip(Item* item, uint32 sender, uint32 action)
    {
        if (!item)
        {
            return;
        }

        apStone.OnGossipSelect(item, sender, action);
    }

    void AP_Character::CreateAPClient()
    {
        ap.reset();

        std::string host = sConfig.GetArchipelagoServerHost();
        std::string port = std::to_string(sConfig.GetArchipelagoServerPort());

        std::cout << "Connecting to Archipelago server at " << host << ":" << port << " with UUID " << uuid << std::endl;
        ChatHandler(player->GetSession()).SendSysMessage("Connecting to Archipelago server...");

        ap = std::make_unique<Network::Client>(sArchipelaWoW->GetWebSocketService(), uuid, AP_GAME_NAME, host, port);
        AddArchipelagoClientHandlers();
    }

    void AP_Character::AddArchipelagoClientHandlers()
    {
        if (!ap)
        {
            return;
        }

        ap->SetSlotConnectedHandler([this](const auto& d) { APSlotConnectedHandler(d); });
        ap->SetSocketErrorHandler([this](const auto& e) { APSocketErrorHandler(e); });
        ap->SetSocketDisconnectedHandler([this]() { APSocketDisconnectedHandler(); });
        ap->SetSlotDisconnectedHandler([this]() { APSlotDisconnectedHandler(); });
        ap->SetBouncedHandler([this](const auto& p) { APBouncedHandler(p); });
        ap->SetRoomInfoHandler([this]() { APRoomInfoHandler(); });
        ap->SetDataPackageChangedHandler([this](const auto& d) { APDataPackageHandler(d); });
        ap->SetItemsReceivedHandler([this](const auto& i) { APReceivedItemsHandler(i); });
        ap->SetPrintJsonHandler([this](const auto& msg) { APPrintJsonHandler(msg); });
        ap->SetSlotRefusedHandler([this](const auto& e) { APSlotRefusedHandler(e); });
    }

    void AP_Character::ConnectAPSlot()
    {
        if (!ap)
        {
            return;
        }

        std::cout << "Connecting to Archipelago slot..." << std::endl;
        ChatHandler(player->GetSession()).SendSysMessage(fmt::format("Connecting to Archipelago slot |cFFFF00FF{}|r...", slot));

        std::string password = sConfig.GetArchipelagoPassword();
        ap->ConnectSlot(slot, password, 0b111, tags);
    }

    AP_Character* AP_Character::LoadFromDatabase(Player* player)
    {
        if (!player)
        {
            return nullptr;
        }

        QueryResult result = CharacterDatabase.Query(
            "SELECT `uuid`, `slot`, `itemIndex`, `apLevel`, `apExp`, `goalCompleted` FROM `ap_character` WHERE `guid` = {}",
            player->GetGUID().GetCounter()
        );
        if (!result)
        {
            return nullptr;
        }

        std::string uuid = (*result)[0].Get<std::string>();
        std::string slot = (*result)[1].Get<std::string>();
        int itemIndex = (*result)[2].Get<int>();
        uint8 level = (*result)[3].Get<uint8>();
        uint32 xp = (*result)[4].Get<uint32>();
        bool goal = (*result)[5].Get<bool>();
        return new AP_Character(player, uuid, slot, itemIndex, level, xp, goal);
    }

    bool AP_Character::IsSlotBound(const std::string& slot)
    {
        std::string escapedSlot(slot);
        CharacterDatabase.EscapeString(escapedSlot);
        QueryResult result = CharacterDatabase.Query("SELECT `guid` FROM `ap_character` WHERE `slot` = '{}'", escapedSlot);
        return !!result;
    }

    void AP_Character::SaveToDatabase()
    {
        if (!player)
        {
            return;
        }

        std::string escapedSlot(slot);
        CharacterDatabase.EscapeString(escapedSlot);
        CharacterDatabase.Execute(
            "REPLACE INTO `ap_character` (`guid`, `uuid`, `slot`, `itemIndex`, `apLevel`, `apExp`, `goalCompleted`) VALUES ({}, '{}', '{}', {}, {}, {}, {})",
            player->GetGUID().GetCounter(), uuid, escapedSlot, itemIndex, apLevel, apExp, goalCompleted
        );

        nextSave = GameTime::GetGameTime() + std::chrono::seconds(15);
    }

    void AP_Character::SyncLocationChecks()
    {
        if (!player || !ap)
        {
            return;
        }

        QueryResult result = CharacterDatabase.Query(
            "SELECT `locationId` FROM `ap_location_check` WHERE `guid` = {}",
            player->GetGUID().GetCounter()
        );
        if (result)
        {
            std::list<int64> locationChecks;
            do
            {
                Field* fields = result->Fetch();
                int32 locationId = fields[0].Get<int32>();
                locationChecks.push_back(locationId);
            } while (result->NextRow());

            ap->LocationChecks(locationChecks);
        }

        if (goalCompleted)
        {
            ap->StatusUpdate(Network::Client::ClientStatus::Goal);
        }
    }

    void AP_Character::RewardItem(int64_t itemId, bool alreadyRewarded, int sender)
    {
        auto item = items.items.GetWoWItemId(itemId);
        if (item.has_value())
        {
            if (alreadyRewarded)
            {
                return;
            }
            const ItemTemplate* itemTemplate = sObjectMgr->GetItemTemplate(item.value());
            if (!itemTemplate)
            {
                std::cerr << "Invalid item ID " << item.value() << " received from Archipelago item " << itemId << "!" << std::endl;
                return;
            }

            MailSender mailSender(MAIL_NORMAL, 0, MAIL_STATIONERY_DEFAULT);
            if (sender == ap->GetPlayerNumber())
            {
                mailSender = MailSender(player);
            }
            else
            {
                std::string senderName = ap->GetPlayerAlias(sender);
                uint32 creature = sArchipelaWoW->GetCreatureTemplateForPlayer(senderName);
                mailSender = MailSender(MAIL_CREATURE, creature);
            }

            Item* reward = Item::CreateItem(itemTemplate->ItemId, 1, player);
            if (!reward)
            {
                std::cerr << "Failed to create item " << itemTemplate->ItemId << " (" << itemTemplate->Name1 << ") for reward!" << std::endl;
                return;
            }

            CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
            reward->SaveToDB(trans);
            MailDraft(itemTemplate->Name1, "")
                .AddItem(reward)
                .SendMailTo(trans, MailReceiver(player, player->GetGUID().GetCounter()), mailSender);
            CharacterDatabase.CommitTransaction(trans);
            return;
        }

        auto zone = items.zones.GetZone(itemId);
        if (zone.has_value())
        {
            unlockedZones.insert(zone.value().id);
            return;
        }

        if (itemId == items.levels)
        {
            if (!alreadyRewarded)
            {
                player->GiveLevel(player->GetLevel() + 1);
            }
            return;
        }
    }

    void AP_Character::CheckIsInLockedZone()
    {
        auto now = GameTime::GetGameTime();
        if (!itemsSynced || now < nextLockedZoneCheck)
        {
            return;
        }

        uint32 zone = player->GetZoneId();
        bool locked = items.zones.IsRestricted(zone) && !IsZoneUnlocked(zone);
        nextLockedZoneCheck = now + std::chrono::seconds(1);

        if (!locked)
        {
            return;
        }

        const AreaTableEntry* area = sAreaTableStore.LookupEntry(zone);
        if (area)
        {
            auto locale = player->GetSession()->GetSessionDbcLocale();
            ChatHandler(player->GetSession()).SendSysMessage(fmt::format("|cFFFF0000{} is locked! Teleporting back...", area->area_name[locale]));
        }
        else
        {
            ChatHandler(player->GetSession()).SendSysMessage("|cFFFF0000Entering locked zone! Teleporting back...");
        }

        lastUnlockedPosition.time = GameTime::GetGameTime();
        nextLockedZoneCheck = now + std::chrono::seconds(6);
        player->CastSpell(player, TRESPASSER_SPELL_ID, true); // Teleport visual effect + stun
        player->TeleportTo(lastUnlockedPosition.mapId, lastUnlockedPosition.x, lastUnlockedPosition.y, lastUnlockedPosition.z, lastUnlockedPosition.orientation);
    }

    void AP_Character::SavePosition()
    {
        if (GameTime::GetGameTime() - lastUnlockedPosition.time < std::chrono::seconds(15))
        {
            return;
        }

        uint32 zone = player->GetZoneId();
        bool locked = items.zones.IsRestricted(zone) && !IsZoneUnlocked(zone);
        if (!locked)
        {
            lastUnlockedPosition = PlayerPosition(player->GetMapId(), player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetOrientation());
        }
    }

    void AP_Character::LoadXPForLevel()
    {
        xpForLevel = 0;

        QueryResult result = WorldDatabase.Query("SELECT `Experience` FROM `player_xp_for_level` WHERE `Level` = {}", apLevel);
        if (!result)
        {
            return;
        }

        xpForLevel = (*result)[0].Get<uint32>();
    }

    void AP_Character::CheckLocation(int32 locationId)
    {
        ap->LocationChecks({ locationId });
        CharacterDatabase.Execute(
            "REPLACE INTO `ap_location_check` (`guid`, `locationId`) VALUES ({}, {})",
            player->GetGUID().GetCounter(), locationId
        );
    }

    void AP_Character::APSlotConnectedHandler(const nlohmann::json& data)
    {
        if (!ap)
        {
            return;
        }

        std::cout << "Slot data: " << std::endl << data.dump() << std::endl;

        if (data["options"]["character_class"] != player->getClass())
        {
            const ChrClassesEntry* slotClass = sChrClassesStore.LookupEntry(data["options"]["character_class"]);
            if (slotClass)
            {
                auto locale = player->GetSession()->GetSessionDbcLocale();
                std::string msg = fmt::format("|cFFFF0000This slot requires a {}! Disconnecting slot because of character class mismatch.", slotClass->name[locale]);
                ChatHandler(player->GetSession()).SendSysMessage(msg);
            }
            run = false;
        }

        if (data["options"]["character_race"] != player->getRace(true))
        {
            const ChrRacesEntry* slotRace = sChrRacesStore.LookupEntry(data["options"]["character_race"]);
            if (slotRace)
            {
                auto locale = player->GetSession()->GetSessionDbcLocale();
                std::string msg = fmt::format("|cFFFF0000This slot requires a {}! Disconnecting slot because of character race mismatch.", slotRace->name[locale]);
                ChatHandler(player->GetSession()).SendSysMessage(msg);
            }
            run = false;
        }

        if (!run)
        {
            return;
        }

        deathLinkEnabled = data["options"]["death_link"] == 1;
        if (deathLinkEnabled && std::find(tags.begin(), tags.end(), "DeathLink") == tags.end())
        {
            tags.push_back("DeathLink");
            ap->ConnectUpdate(0b111, tags);
        }

        for (const auto& level : data["locations"]["levels"])
        {
            locations.levels.AddLocation(level[0], level[1]);
        }
        for (const auto& quest : data["locations"]["quests"])
        {
            locations.quests.AddLocation(quest[0], quest[1]);
        }
        for (const auto& achievement : data["locations"]["achievements"])
        {
            locations.achievements.AddLocation(achievement[0], achievement[1]);
        }
        for (const auto& fp : data["locations"]["flightpaths"])
        {
            locations.flightPaths.AddLocation(fp[0], fp[1]);
        }

        for (const auto& item : data["items"]["zones"])
        {
            items.zones.AddItem(item[0], item[1], item[2], item[3], item[4][0], item[4][1], item[4][2], item[4][3], item[4][4]);
        }
        for (const auto& item : data["items"]["items"])
        {
            items.items.AddItem(item[0], item[1]);
        }
        items.levels = data["items"]["levels"];

        goalAchievementId = data["goal"];
        maxLevel = data["maxlevel"];

        apStone.CreateItem();
        SaveToDatabase();
        SyncLocationChecks();
    }

    void AP_Character::APSocketErrorHandler(const std::string& error)
    {
        std::cerr << "APSocketErrorHandler: " << error << std::endl;
    }

    void AP_Character::APSocketDisconnectedHandler()
    {
        std::cerr << "Disconnected from Archipelago server" << std::endl;
        ChatHandler(player->GetSession()).SendSysMessage("|cFFFF0000Disconnected from Archipelago server");
    }

    void AP_Character::APSlotDisconnectedHandler()
    {
        std::cerr << "Disconnected from Archipelago slot" << std::endl;
        ChatHandler(player->GetSession()).SendSysMessage("|cFFFF0000Disconnected from Archipelago slot");
    }

    void AP_Character::APBouncedHandler(const nlohmann::json& packet)
    {
        std::list<std::string> tags = packet["tags"];

        bool deathlink = (std::find(tags.begin(), tags.end(), "DeathLink") != tags.end());
        if (deathlink && deathLinkEnabled)
        {
            auto& data = packet["data"];
            std::string cause = "";
            if (data.contains("cause"))
            {
                cause = data["cause"];
            }
            std::string source = "";
            if (data.contains("source"))
            {
                source = data["source"];
            }
            double timestamp = data["time"];

            if (timestamp > lastDeathTime && player->IsAlive())
            {
                lastDeathTime = timestamp;
                std::string message = fmt::format("You have been killed by |cFFFFF380{}|r", source, cause);
                if (!cause.empty())
                {
                    message += fmt::format(" ({})", cause);
                }
                ChatHandler(player->GetSession()).SendSysMessage(message);
                player->Kill(player, player, false);
            }
        }
    }

    void AP_Character::APRoomInfoHandler()
    {
        if (!ap)
        {
            return;
        }

        std::cout << "APRoomInfoHandler" << std::endl;
        ap->GetDataPackage({ AP_GAME_NAME });
    }

    void AP_Character::APDataPackageHandler(const nlohmann::json& data)
    {
        if (!ap)
        {
            return;
        }

        std::cout << "Data package:" << std::endl << data.dump() << std::endl;

        if (ap->GetState() < Network::Client::State::SlotConnected)
        {
            ConnectAPSlot();
        }
    }

    void AP_Character::APReceivedItemsHandler(const std::list<Network::Client::NetworkItem>& items)
    {
        std::cout << "Received items:" << std::endl;

        int newItemIndex = itemIndex;

        for (const auto& item : items)
        {
            bool alreadyRewarded = itemIndex >= item.index;
            int sender = item.player;

            if (item.index > newItemIndex)
            {
                newItemIndex = item.index;
            }

            std::cout << "  - Item index: " << item.index << ", sender: " << sender << ", item ID : " << item.item << ", flags : " << item.flags << ", already rewarded : " << alreadyRewarded << std::endl;
            RewardItem(item.item, alreadyRewarded, sender);
        }

        if (newItemIndex != itemIndex)
        {
            itemIndex = newItemIndex;
            SaveToDatabase();
        }

        itemsSynced = true;
    }

    void AP_Character::APPrintJsonHandler(const std::list<Network::Client::TextNode>& msg)
    {
        if (!ap || !run)
        {
            return;
        }

        std::string str = ap->RenderJson(msg, Network::Client::RenderFormat::Ansi);
        str = ConvertANSIColoredString(str);
        ChatHandler(player->GetSession()).SendSysMessage(str);
    }

    void AP_Character::APSlotRefusedHandler(const std::list<std::string>& errors)
    {
        if (!player)
        {
            return;
        }

        std::string joined;
        bool first = true;
        for (const auto& s : errors) {
            if (!first)
            {
                joined += ", ";
            }
            joined += s;
            first = false;
        }

        std::cerr << "Connection to slot " << slot << " refused: " << joined << std::endl;
        ChatHandler(player->GetSession()).SendSysMessage(fmt::format("|cFFFF0000Connection to slot |cFFFF00FF{}|cFFFF0000 refused: {}|r", slot, joined));

        run = false;
    }

    std::string AP_Character::ConvertANSIColoredString(const std::string& str)
    {
        // Map ANSI codes to |cAARRGGBB format
        static const std::unordered_map<std::string, std::string> ansiToHex = {
            {"\x1b[31m", "|cFFFF0000"}, // red
            {"\x1b[32m", "|cFF00FF00"}, // green
            {"\x1b[33m", "|cFFFFF380"}, // yellow
            {"\x1b[34m", "|cFF0050FF"}, // blue
            {"\x1b[35m", "|cFFFF00FF"}, // magenta
            {"\x1b[36m", "|cFF00FFFF"}, // cyan
            {"\x1b[38:5:219m", "|cFFFF77FF"}, // plum
            {"\x1b[38:5:62m",  "|cFF5F5FD7"}, // slateblue
            {"\x1b[38:5:210m", "|cFFFF8787"}, // salmon
            {"\x1b[90m", "|cFF808080"}, // gray/grey
            {"\x1b[0m", "|r"} // reset
        };

        std::string result;
        result.reserve(str.size());

        for (size_t i = 0; i < str.size(); )
        {
            bool matched = false;

            for (const auto& [ansi, replacement] : ansiToHex)
            {
                if (str.compare(i, ansi.size(), ansi) == 0)
                {
                    result += replacement;
                    i += ansi.size();
                    matched = true;
                    break;
                }
            }

            if (!matched)
            {
                result += str[i];
                ++i;
            }
        }

        return result;
    }

    std::string AP_Character::GenerateUUID()
    {
        QueryResult result = CharacterDatabase.Query("SELECT UUID()");
        if (!result)
        {
            return "";
        }

        return (*result)[0].Get<std::string>();
    }
}
