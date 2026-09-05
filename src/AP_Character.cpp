#include "AP_Character.h"
#include "AP_PlayerPosition.h"
#include "ArchipelaWoW.h"
#include "Chat.h"
#include "DatabaseEnv.h"
#include "DatabaseEnvFwd.h"
#include "DBCEnums.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "Define.h"
#include "Errors.h"
#include "Field.h"
#include "fmt/format.h"
#include "GameTime.h"
#include "GossipDef.h"
#include "Item.h"
#include "items/AP_Gear.h"
#include "items/AP_GearPool.h"
#include "items/AP_ItemsContainer.h"
#include "items/AP_Progressive.h"
#include "items/AP_Spells.h"
#include "items/AP_Zones.h"
#include "ItemTemplate.h"
#include "Log.h"
#include "Mail.h"
#include "network/AP_Client.h"
#include "nlohmann/json.hpp"
#include "NPCPackets.h"
#include "ObjectMgr.h"
#include "Optional.h"
#include "Player.h"
#include "QuestDef.h"
#include "SpellMgr.h"
#include "Trainer.h"
#include "Unit.h"

#include <algorithm>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <list>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#define sConfig sArchipelaWoW->GetConfig()
constexpr auto AP_GAME_NAME = "World of Warcraft";
constexpr uint32 TRESPASSER_SPELL_ID = 73536; // Trespasser - spell with teleport visual effect and short stun, used when player tries to enter a locked zone
constexpr uint32 TELEPORT_SPELL_ID = 7141; // Simple Teleport - visual effect, used when teleporting with the Archipelago Stone
// "Archipelago Movement Speed", the serverside-only passive the module adds to `spell_dbc`. It is
// passive, so the client is never told about it and never has to know the id.
constexpr uint32 MOVEMENT_SPEED_SPELL_ID = 100500;
// Stands in until the slot sends its own, which it does before any item can arrive
constexpr uint8 DEFAULT_GEAR_REWARD_LEVEL_WINDOW = 3;
// Stands in for the taught list of an entry the slot does not carry, so GrantSpell can bind a
// reference either way
static const std::vector<uint32> EMPTY_SPELL_LIST;

namespace ModArchipelaWoW
{
    AP_Character::AP_Character(Player* player, std::string slot) :
        player(player),
        apStone(this),
        slot(slot),
        itemIndex(-1),
        countedItemIndex(-1),
        lastDeathTime(-1.0),
        deathLinkEnabled(false),
        itemsSynced(false),
        run(true),
        tags(),
        items(),
        locations(),
        unlockedZones(),
        grantedSpells(),
        checkedLocations(),
        progressiveCounts(),
        lastUnlockedPosition(player->GetMapId(), player->GetPositionX(), player->GetPositionY(), player->GetPositionZ(), player->GetOrientation()),
        nextLockedZoneCheck(GameTime::GetGameTime() + std::chrono::seconds(6)),
        nextSave(GameTime::GetGameTime() + std::chrono::seconds(15)),
        goalAchievementId(0),
        gearRewardLevelWindow(DEFAULT_GEAR_REWARD_LEVEL_WINDOW),
        gearAllArmorTypes(false),
        maxLevel(0),
        apLevel(1),
        apExp(0),
        xpForLevel(0),
        goalCompleted(false),
        grantingSpells()
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

    uint32 AP_Character::GetGoldPouchAmount() const
    {
        if (!player)
        {
            return 0;
        }

        uint8 level = player->GetLevel();
        level = std::max(uint8(5), level);

        const double lvl = static_cast<double>(level);
        const double multiplier = 1.5;

        double result;
        if (level <= 60)
        {
            result = 6.0731 * std::pow(lvl, 2.0641);
        }
        else
        {
            result = 2.3e-6 * std::pow(lvl, 5.9408);
        }
        result *= multiplier;

        return static_cast<uint32>(std::round(result));
    }

    void AP_Character::SendLevelReport() const
    {
        ChatHandler chat(player->GetSession());

        // maxLevel arrives with the slot data. Until it does, OnPlayerGiveXP treats the character
        // as capped and banks nothing, so saying nothing about it would be misleading.
        if (maxLevel == 0)
        {
            chat.SendSysMessage(fmt::format("Archipelago level |cFF4CFF00{}|r", apLevel));
            chat.SendSysMessage("|cFFFFF380Waiting for slot data from the Archipelago server - experience is not being counted yet.");
            return;
        }

        if (apLevel >= maxLevel)
        {
            chat.SendSysMessage(fmt::format("Archipelago level |cFF4CFF00{}|r of |cFF4CFF00{}|r - level cap reached.", apLevel, maxLevel));
            return;
        }

        chat.SendSysMessage(fmt::format("Archipelago level |cFF4CFF00{}|r of |cFF4CFF00{}|r", apLevel, maxLevel));

        // player_xp_for_level has no row for every level on every core, so do not divide blindly.
        if (xpForLevel == 0)
        {
            chat.SendSysMessage(fmt::format("Experience: |cFF4CFF00{}|r |cFF808080(no requirement on record for this level)|r", apExp));
            return;
        }

        double progress = 100.0 * static_cast<double>(apExp) / static_cast<double>(xpForLevel);
        uint32 remaining = xpForLevel > apExp ? xpForLevel - apExp : 0;

        chat.SendSysMessage(fmt::format("Experience: |cFF4CFF00{}|r / |cFF4CFF00{}|r |cFF5F5FD7({:.1f}%)|r", apExp, xpForLevel, progress));
        chat.SendSysMessage(fmt::format("|cFF4CFF00{}|r experience to level |cFF4CFF00{}|r", remaining, apLevel + 1));
    }

    bool AP_Character::IsSlotConnected() const
    {
        return ap && ap->GetState() >= Network::Client::State::SlotConnected;
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

        auto checkId = locations.quests.GetLocationId(quest);
        if (checkId.has_value())
        {
            CheckLocation(checkId.value());
        }
    }

    void AP_Character::OnPlayerGiveXP(uint32& xp, Unit* /*victim*/, uint8 /*xpSource*/)
    {
        if (apLevel >= maxLevel)
        {
            xp = 0;
            return;
        }

        // The character's own experience bar never moves -- xp is zeroed below -- so the
        // Progressive Experience Rate bonus has to be applied here, to the hidden Archipelago
        // experience that drives apLevel and the level location checks.
        uint32 baseXp = xp;
        uint32 xpBonusPct = GetProgressiveStep(Items::ProgressiveType::ExperienceRate);
        if (xpBonusPct > 0)
        {
            uint64 boosted = static_cast<uint64>(xp) * (100 + xpBonusPct) / 100;
            xp = static_cast<uint32>(std::min<uint64>(boosted, std::numeric_limits<uint32>::max()));
        }

        AnnounceXPGain(baseXp, xp, xpBonusPct);

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

    bool AP_Character::OnPlayerCanLearnSpell(uint32 spellId)
    {
        // A randomized spell is the multiworld's to hand over, so every other way of picking it up
        // -- a trainer, a quest reward, a spell that teaches another -- comes away empty.
        return grantingSpells.contains(spellId) || !items.spells.IsRandomized(spellId);
    }

    void AP_Character::OnPlayerGetTrainerSpellState(uint32 spellId, Trainer::SpellState& state)
    {
        Optional<int> locationId = locations.spells.GetLocationId(spellId);
        if (!locationId.has_value())
        {
            return;
        }

        if (checkedLocations.contains(locationId.value()))
        {
            // The check has been sent, so there is nothing left here to buy -- including for a
            // character still waiting on the spell itself to come back from the multiworld.
            state = Trainer::SpellState::Known;
            return;
        }

        if (state != Trainer::SpellState::Known)
        {
            // Whatever the trainer worked out stands: the check is bought the ordinary way.
            return;
        }

        // The multiworld handed the spell over before its check was bought, and the trainer will
        // not sell what a character already knows. The level requirement is all that is left of
        // the trainer's own answer, so it is applied here and the entry goes back on offer.
        Optional<Items::SpellItem> spell = items.spells.GetSpellBySpellId(spellId);
        uint8 reqLevel = spell.has_value() ? spell.value().reqLevel : 0;
        state = player->GetLevel() >= reqLevel
            ? Trainer::SpellState::Available
            : Trainer::SpellState::Unavailable;
    }

    void AP_Character::OnPlayerBeforeReceiveSpellListFromTrainer(WorldPackets::NPC::TrainerList& trainerList)
    {
        // A spent row is dropped from the list rather than sent as known, because a row the client
        // has ever seen is one it can bring back: it redraws an open trainer window from its own
        // spellbook, and a check never adds to that, so every spent row turns green again the next
        // time anything at all is learned -- an Archipelago item landing is enough. A row that was
        // never in the packet has nothing to redraw from.
        std::erase_if(trainerList.Spells, [this](const WorldPackets::NPC::TrainerListSpell& spell)
        {
            Optional<int> locationId = locations.spells.GetLocationId(spell.SpellID);
            return locationId.has_value() && checkedLocations.contains(locationId.value());
        });
    }

    void AP_Character::OnPlayerAfterTrainSpell(Creature* trainer, uint32 spellId)
    {
        Optional<int> locationId = locations.spells.GetLocationId(spellId);
        if (!locationId.has_value() || checkedLocations.contains(locationId.value()))
        {
            return;
        }

        CheckLocation(locationId.value());
        ReopenTrainerWindow(trainer);
    }

    void AP_Character::ReopenTrainerWindow(Creature* trainer)
    {
        if (!trainer || !player)
        {
            return;
        }

        // A check teaches nothing, and the client works an open trainer window out from its own
        // spellbook: the row just bought stays on offer, and it throws away any spell list the
        // server sends it uninvited. The one list it does honour is the one it asked for itself,
        // so the trainer is handed back the way talking to it does -- except that the menu is cut
        // down to the training option alone. A single-option menu is auto-selected by the client,
        // which turns into the request that gets the window redrawn; leaving the other options on
        // it (talent unlearning, from level 10) would stop at the menu and cost a manual click.
        //
        // The quests are left out for the same reason. Most class and mount trainers are quest
        // givers as well, and SendGossipMenu writes whatever the quest menu holds into the same
        // packet -- so a trainer with a quest to offer would arrive as more than one entry and stop
        // at the menu again. Nothing here wants them: the menu is thrown away either way.
        player->PrepareGossipMenu(trainer, trainer->GetGossipMenuId(), false);

        GossipMenu& menu = player->PlayerTalkClass->GetGossipMenu();

        // The menu is keyed by option id rather than laid out as a run of indices, and the ids come
        // from the database, so the option is looked for over what the menu actually holds.
        const GossipMenuItem* trainingOption = nullptr;
        const GossipMenuItemData* trainingData = nullptr;
        for (const auto& [itemId, item] : menu.GetMenuItems())
        {
            if (item.OptionType == GOSSIP_OPTION_TRAINER)
            {
                trainingOption = &item;
                trainingData = menu.GetItemData(itemId);
                break;
            }
        }

        if (!trainingOption)
        {
            // Nothing to cut the menu down to, so hand over whatever the creature would have shown
            player->SendPreparedGossip(trainer);
            return;
        }

        // Copied out before the menu is emptied, which invalidates both pointers
        uint8 icon = trainingOption->MenuItemIcon;
        std::string message = trainingOption->Message;
        uint32 sender = trainingOption->Sender;
        uint32 actionMenuId = trainingData ? trainingData->GossipActionMenuId : 0;
        uint32 actionPoi = trainingData ? trainingData->GossipActionPoi : 0;

        uint32 textId = player->GetGossipTextId(trainer);
        if (uint32 menuId = menu.GetMenuId())
        {
            textId = player->GetGossipTextId(menuId, trainer);
        }

        // The data goes back alongside the option: Player::OnGossipSelect drops any selection whose
        // item has none, which would leave the client waiting on a reply that never comes and the
        // creature unable to be talked to again until it is out of range.
        constexpr uint32 onlyItemId = 0;
        menu.ClearMenu();
        menu.AddMenuItem(onlyItemId, icon, message, sender, GOSSIP_OPTION_TRAINER, "", 0);
        menu.AddGossipMenuItemData(onlyItemId, actionMenuId, actionPoi);
        player->PlayerTalkClass->SendGossipMenu(textId, trainer->GetGUID());
    }

    void AP_Character::OnPlayerAfterTakeItemFromMail(uint32 wowItemId)
    {
        // Collecting a tier from the mailbox is the moment a character can end up holding two rungs
        // of the same ladder, and it is the only moment outside an item sync that it happens. Every
        // other mail -- gear, a gold pouch -- leaves nothing to tidy up, so it costs a lookup here
        // rather than a bag sweep.
        for (Items::ProgressiveType type : { Items::ProgressiveType::Food, Items::ProgressiveType::Drink })
        {
            const Items::ProgressiveItem* progressive = items.progressive.GetProgressiveItemByType(type);
            if (!progressive)
            {
                continue;
            }

            const std::vector<uint32>& steps = progressive->steps;
            if (std::find(steps.begin(), steps.end(), wowItemId) != steps.end())
            {
                RemoveOutgrownProgressiveItems(type);
                return;
            }
        }
    }

    void AP_Character::OnPlayerCreateItem(Item* item)
    {
        apStone.OnPlayerCreateItem(item);
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

    void AP_Character::GrantSpell(uint32 spellId)
    {
        if (!player)
        {
            return;
        }

        // Recorded whether or not the spell has to be taught now, because this is also what tells
        // RemoveUngrantedSpells that the character came by it honestly.
        grantedSpells.insert(spellId);

        Optional<Items::SpellItem> spell = items.spells.GetSpellBySpellId(spellId);
        const std::vector<uint32>& taughtSpells = spell.has_value() ? spell.value().taughtSpells : EMPTY_SPELL_LIST;

        if (taughtSpells.empty())
        {
            if (player->HasSpell(spellId))
            {
                return;
            }

            grantingSpells = { spellId };
            player->learnSpell(spellId);
            grantingSpells.clear();
            return;
        }

        // A wrapper is cast rather than learned, the same way the trainer would hand it over, and
        // that cast teaches everything hanging off it. Anything with an item of its own is left out
        // of the pass: a paladin buying Judgement must not come away with Seal of Righteousness too,
        // and a class mount must not carry Apprentice Riding in with it.
        bool anythingMissing = false;
        for (uint32 taughtSpell : taughtSpells)
        {
            if (items.spells.HasItemOfItsOwn(taughtSpell))
            {
                continue;
            }

            grantedSpells.insert(taughtSpell);
            grantingSpells.insert(taughtSpell);
            anythingMissing = anythingMissing || !player->HasSpell(taughtSpell);
        }

        if (anythingMissing)
        {
            player->CastSpell(player, spellId, true);
        }

        grantingSpells.clear();
    }

    void AP_Character::RemoveSpellIfUngranted(uint32 spellId)
    {
        if (grantedSpells.contains(spellId) || !player->HasSpell(spellId))
        {
            return;
        }

        player->removeSpell(spellId, SPEC_MASK_ALL, false);
    }

    void AP_Character::RemoveUngrantedSpells()
    {
        if (!player)
        {
            return;
        }

        // A character bound to a slot after it was rolled up keeps whatever it had already learned,
        // and with the starter abilities option on it starts out knowing several of them outright.
        // Anything randomized that the multiworld has not handed over is taken back, higher ranks
        // included -- removeSpell walks the chain -- so the trainer has something left to sell.
        for (auto itr = items.spells.Begin(); itr != items.spells.End(); ++itr)
        {
            uint32 spellId = itr->second.spellId;
            RemoveSpellIfUngranted(spellId);
            for (uint32 taughtSpell : itr->second.taughtSpells)
            {
                RemoveSpellIfUngranted(taughtSpell);
            }
        }
    }

    void AP_Character::CreateAPClient()
    {
        std::string host = sConfig.GetArchipelagoServerHost();
        std::string port = std::to_string(sConfig.GetArchipelagoServerPort());

        LOG_INFO("module.archipelawow", "Connecting to Archipelago server at {}:{} with UUID {}", host, port, uuid);
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
        ap->SetBouncedHandler([this](const auto& p) { APBouncedHandler(p); });
        ap->SetRoomInfoHandler([this]() { APRoomInfoHandler(); });
        ap->SetDataPackageChangedHandler([this](const auto& d) { APDataPackageHandler(d); });
        ap->SetItemsReceivedHandler([this](const auto& i) { APReceivedItemsHandler(i); });
        ap->SetPrintJsonHandler([this](const auto& msg) { APPrintJsonHandler(msg); });
        ap->SetSlotRefusedHandler([this](const auto& e) { APSlotRefusedHandler(e); });
        ap->SetMessageErrorHandler([this](const auto& e) { APMessageErrorHandler(e); });
    }

    void AP_Character::ConnectAPSlot()
    {
        if (!ap)
        {
            return;
        }

        LOG_INFO("module.archipelawow", "Connecting to Archipelago slot {}...", slot);
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
                checkedLocations.insert(locationId);
            } while (result->NextRow());

            ap->LocationChecks(locationChecks);
        }

        if (goalCompleted)
        {
            ap->StatusUpdate(Network::Client::ClientStatus::Goal);
        }
    }

    void AP_Character::RewardItem(int64_t itemId, bool alreadyRewarded, bool alreadyCounted, int sender)
    {
        auto item = items.items.GetWoWItemId(itemId);
        if (item.has_value())
        {
            if (!alreadyRewarded)
            {
                MailItemReward(item.value(), itemId, sender);
            }
            return;
        }

        auto gear = items.gear.GetGearItem(itemId);
        if (gear.has_value())
        {
            if (!alreadyRewarded)
            {
                // Rolled here rather than upstream when the seed was built. Which piece fits depends
                // on the level the character is at when the item lands and on what its class can
                // wear, and the multiworld knows neither when it generates.
                Optional<uint32> rolled = sArchipelaWoW->GetGearPool().Roll(
                    gear.value().category, gear.value().quality, player,
                    gearRewardLevelWindow, gearAllArmorTypes, maxLevel
                );
                if (!rolled)
                {
                    LOG_ERROR("module.archipelawow",
                        "No gear candidate for Archipelago item {} (category {}, quality {}) for a level {} class {}!",
                        itemId, static_cast<uint32>(gear.value().category), gear.value().quality,
                        player->GetLevel(), static_cast<uint32>(player->getClass()));
                    return;
                }

                MailItemReward(rolled.value(), itemId, sender);
            }
            return;
        }

        auto zone = items.zones.GetZone(itemId);
        if (zone.has_value())
        {
            unlockedZones.insert(zone.value().id);

            for (uint32 itemId : zone.value().keys)
            {
                if (!player->HasItemCount(itemId, 1))
                {
                    player->AddItem(itemId, 1);
                }
            }

            return;
        }

        auto spell = items.spells.GetSpellByItemId(itemId);
        if (spell.has_value())
        {
            // Granted on every pass rather than only the first: unlike a mailed reward, a spell can
            // go missing from a character -- a spec reset, a hand-edited database -- and teaching it
            // again costs nothing when it is already known.
            GrantSpell(spell.value().spellId);
            return;
        }

        auto progressive = items.progressive.GetProgressiveItem(itemId);
        if (progressive.has_value())
        {
            const Items::ProgressiveItem& track = progressive.value();

            // Unlike the one-shot rewards above, a progressive track is worth the sum of every copy
            // the slot has ever received, so the copies handed out in earlier sessions have to be
            // counted again after a relog. `alreadyCounted` only keeps the same copy from being
            // counted twice when the server replays the item list.
            uint32& count = progressiveCounts[track.type];
            if (!alreadyCounted)
            {
                ++count;
            }

            if (track.type == Items::ProgressiveType::Riding)
            {
                // Every rank up to the count rather than only the newest, and on every pass rather
                // than only on a fresh copy: the item list is replayed on each connection, and the
                // ranks a character already learned have to be recorded again or the sweep below
                // would take them back off it.
                for (uint32 rank = 0; rank < count && rank < track.steps.size(); ++rank)
                {
                    GrantSpell(track.steps[rank]);
                }

                return;
            }

            // The food and drink tracks pay out a real item per copy rather than a percentage
            bool grantsItem = track.type == Items::ProgressiveType::Food || track.type == Items::ProgressiveType::Drink;
            if (grantsItem && !alreadyRewarded && count > 0 && count <= track.steps.size())
            {
                MailItemReward(track.steps[count - 1], itemId, sender);
            }

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

        if (itemId == items.goldPouch)
        {
            if (!alreadyRewarded)
            {
                MailSender mailSender = GetMailSender(sender);
                CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
                MailDraft("Gold Pouch", "")
                    .AddMoney(GetGoldPouchAmount())
                    .SendMailTo(trans, MailReceiver(player, player->GetGUID().GetCounter()), mailSender);
                CharacterDatabase.CommitTransaction(trans);
            }
            return;
        }
    }

    void AP_Character::AnnounceXPGain(uint32 baseXp, uint32 totalXp, uint32 bonusPct) const
    {
        if (totalXp == 0)
        {
            return;
        }

        // Nothing the player can see moves when they earn experience, so without this there is no
        // way to tell a worthwhile kill from a worthless one -- or that anything was earned at all.
        std::string message = fmt::format("Gained |cFF4CFF00{}|r Archipelago XP", totalXp);
        if (bonusPct > 0)
        {
            message += fmt::format(" |cFF808080(base: {} XP, bonus +{}%: {} XP)|r", baseXp, bonusPct, totalXp - baseXp);
        }

        ChatHandler(player->GetSession()).SendSysMessage(message);
    }

    uint32 AP_Character::GetProgressiveStep(Items::ProgressiveType type) const
    {
        auto count = progressiveCounts.find(type);
        if (count == progressiveCounts.end())
        {
            return 0;
        }

        const Items::ProgressiveItem* progressive = items.progressive.GetProgressiveItemByType(type);
        if (!progressive)
        {
            return 0;
        }

        return progressive->ValueAt(count->second);
    }

    void AP_Character::ApplyMovementSpeedBonus()
    {
        if (!player)
        {
            return;
        }

        player->RemoveAurasDueToSpell(MOVEMENT_SPEED_SPELL_ID);

        int32 bonus = static_cast<int32>(GetProgressiveStep(Items::ProgressiveType::MovementSpeed));
        if (bonus <= 0)
        {
            return;
        }

        if (!sSpellMgr->GetSpellInfo(MOVEMENT_SPEED_SPELL_ID))
        {
            LOG_ERROR("module.archipelawow",
                "Spell {} is missing, so the Progressive Movement Speed bonus cannot be granted. "
                "The module's world database updates have not been applied.", MOVEMENT_SPEED_SPELL_ID);
            return;
        }

        // One aura carrying three effects: on foot, on a ground mount and on a flying mount. All
        // three are of the "always" kind, which the core multiplies into the speed it has already
        // worked out, so the bonus rides on top of mount speed instead of being swallowed by it.
        player->CastCustomSpell(player, MOVEMENT_SPEED_SPELL_ID, &bonus, &bonus, &bonus, true);
    }

    void AP_Character::RemoveOutgrownProgressiveItems(Items::ProgressiveType type)
    {
        if (!player)
        {
            return;
        }

        const Items::ProgressiveItem* progressive = items.progressive.GetProgressiveItemByType(type);
        if (!progressive)
        {
            return;
        }

        // An eternal food or drink is strictly better than every tier below it, and the character
        // cannot bin the old ones itself -- they are ITEM_FLAG_NO_USER_DESTROY -- so left alone
        // they would cost a bag slot per step for the rest of the seed.
        const std::vector<uint32>& steps = progressive->steps;

        // What matters is the best tier the character already holds, not the best one it has been
        // sent: a newer tier may still be sitting in the mail, and taking away the one it is living
        // on until it gets round to collecting would defeat the point of mailing rewards.
        //
        // The bank counts as holding it, on both passes. A character that parks its outgrown tiers
        // there to keep its bags clear still expects them cleared out, and cannot bin them itself:
        // they are ITEM_FLAG_NO_USER_DESTROY, and HandleSellItemOpcode refuses an item worth no
        // money. DestroyItemCount searches the bags before the bank, and maxcount 1 means there is
        // only ever the one copy to find.
        Optional<size_t> best;
        for (size_t i = 0; i < steps.size(); ++i)
        {
            if (player->HasItemCount(steps[i], 1, true))
            {
                best = i;
            }
        }

        if (!best.has_value())
        {
            return;
        }

        for (size_t i = 0; i < best.value(); ++i)
        {
            if (player->HasItemCount(steps[i], 1, true))
            {
                player->DestroyItemCount(steps[i], 1, true);
            }
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

        // A DBC localized string can be absent for the session's locale, and handing fmt a null
        // char const* is undefined. The unnamed case already has a message of its own.
        const AreaTableEntry* area = sAreaTableStore.LookupEntry(zone);
        const char* areaName = area ? area->area_name[player->GetSession()->GetSessionDbcLocale()] : nullptr;

        if (areaName && *areaName)
        {
            ChatHandler(player->GetSession()).SendSysMessage(fmt::format("|cFFFF0000{} is locked! Teleporting back...", areaName));
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
        checkedLocations.insert(locationId);
        ap->LocationChecks({ locationId });
        CharacterDatabase.Execute(
            "REPLACE INTO `ap_location_check` (`guid`, `locationId`) VALUES ({}, {})",
            player->GetGUID().GetCounter(), locationId
        );
    }

    void AP_Character::MailItemReward(uint32 wowItemId, int64_t apItemId, int sender)
    {
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(wowItemId);
        if (!itemTemplate)
        {
            LOG_ERROR("module.archipelawow", "Invalid item ID {} received from Archipelago item {}!",
                wowItemId, apItemId);
            return;
        }

        MailSender mailSender = GetMailSender(sender);
        Item* reward = Item::CreateItem(itemTemplate->ItemId, 1, player);
        if (!reward)
        {
            LOG_ERROR("module.archipelawow", "Failed to create item {} ({}) for reward!",
                itemTemplate->ItemId, itemTemplate->Name1);
            return;
        }

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        reward->SaveToDB(trans);
        MailDraft(itemTemplate->Name1, "")
            .AddItem(reward)
            .SendMailTo(trans, MailReceiver(player, player->GetGUID().GetCounter()), mailSender);
        CharacterDatabase.CommitTransaction(trans);
    }

    MailSender AP_Character::GetMailSender(int sender)
    {
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

        return mailSender;
    }

    void AP_Character::APSlotConnectedHandler(const nlohmann::json& data)
    {
        if (!ap)
        {
            return;
        }

        LOG_DEBUG("module.archipelawow", "Slot data: {}", data.dump());

        // Parse everything before acting on it; a missing or mistyped field
        // (e.g. apworld/module version mismatch) throws and disconnects the
        // slot instead of leaving a half-initialized character.
        try
        {
            const auto& options = data.at("options");

            uint32 requiredClass = options.at("character_class").get<uint32>();
            if (requiredClass != player->getClass())
            {
                const ChrClassesEntry* slotClass = sChrClassesStore.LookupEntry(requiredClass);
                if (slotClass)
                {
                    auto locale = player->GetSession()->GetSessionDbcLocale();
                    std::string msg = fmt::format("|cFFFF0000This slot requires a {}! Disconnecting slot because of character class mismatch.", slotClass->name[locale]);
                    ChatHandler(player->GetSession()).SendSysMessage(msg);
                }
                run = false;
            }

            uint32 requiredRace = options.at("character_race").get<uint32>();
            if (requiredRace != player->getRace(true))
            {
                const ChrRacesEntry* slotRace = sChrRacesStore.LookupEntry(requiredRace);
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

            deathLinkEnabled = options.at("death_link") == 1;

            // Per slot rather than per server: two players sharing a worldserver can run different
            // window widths, so the value travels with the slot instead of sitting in the config.
            uint32 levelWindow = options.at("gear_reward_level_window").get<uint32>();
            gearRewardLevelWindow = static_cast<uint8>(std::min<uint32>(levelWindow, STRONG_MAX_LEVEL));
            gearAllArmorTypes = options.at("gear_include_all_armor_types") == 1;

            const auto& locationData = data.at("locations");
            for (const auto& level : locationData.at("levels"))
            {
                locations.levels.AddLocation(level.at(0), level.at(1));
            }
            for (const auto& quest : locationData.at("quests"))
            {
                locations.quests.AddLocation(quest.at(0), quest.at(1));
            }
            for (const auto& achievement : locationData.at("achievements"))
            {
                locations.achievements.AddLocation(achievement.at(0), achievement.at(1));
            }
            for (const auto& fp : locationData.at("flightpaths"))
            {
                locations.flightPaths.AddLocation(fp.at(0), fp.at(1));
            }
            for (const auto& spell : locationData.at("spells"))
            {
                locations.spells.AddLocation(spell.at(0), spell.at(1));
            }

            const auto& itemData = data.at("items");
            for (const auto& item : itemData.at("zones"))
            {
                const auto& pos = item.at(4);
                items.zones.AddItem(item.at(0), item.at(1), item.at(2), item.at(3),
                    pos.at(0), pos.at(1), pos.at(2), pos.at(3), pos.at(4), item.at(5));
            }
            for (const auto& item : itemData.at("items"))
            {
                items.items.AddItem(item.at(0), item.at(1));
            }
            for (const auto& item : itemData.at("gear"))
            {
                uint32 rawCategory = item.at(1).get<uint32>();
                Optional<Items::GearCategory> category = Items::GearCategoryFromValue(rawCategory);
                if (!category)
                {
                    throw std::runtime_error(fmt::format("unknown gear category {}", rawCategory));
                }
                items.gear.AddItem(item.at(0), category.value(), item.at(2));
            }
            for (const auto& item : itemData.at("progressive"))
            {
                uint32 rawType = item.at(1).get<uint32>();
                Optional<Items::ProgressiveType> type = Items::ProgressiveTypeFromValue(rawType);
                if (!type)
                {
                    throw std::runtime_error(fmt::format("unknown progressive type {}", rawType));
                }

                std::vector<uint32> steps = item.at(2).get<std::vector<uint32>>();

                if (type.value() == Items::ProgressiveType::Riding)
                {
                    // The ladder shares one item between its ranks, so the ranks themselves are
                    // registered by spell: that is what tells the trainer hooks these are the seed's
                    // to hand over, and what each one waits on a character reaching. The levels the
                    // slot sends are wanted here and nowhere else, so they go no further.
                    std::vector<uint32> levels = item.at(3).get<std::vector<uint32>>();
                    for (size_t i = 0; i < steps.size(); ++i)
                    {
                        items.spells.AddTrainerSpell(steps[i], i < levels.size() ? static_cast<uint8>(levels[i]) : 0);
                    }
                }

                items.progressive.AddItem(item.at(0), type.value(), std::move(steps));
            }
            for (const auto& item : itemData.at("spells"))
            {
                items.spells.AddItem(item.at(0), item.at(1), item.at(2), item.at(3).get<std::vector<uint32>>());
            }
            items.levels = itemData.at("levels");
            items.goldPouch = itemData.at("money");

            goalAchievementId = data.at("goal");
            maxLevel = data.at("maxlevel");
        }
        catch (const std::exception& ex)
        {
            LOG_ERROR("module.archipelawow", "Invalid slot data received: {}", ex.what());
            ChatHandler(player->GetSession()).SendSysMessage("|cFFFF0000Invalid slot data received from the Archipelago server (apworld/module version mismatch?). Disconnecting.");
            run = false;
            return;
        }

        if (deathLinkEnabled && std::find(tags.begin(), tags.end(), "DeathLink") == tags.end())
        {
            tags.push_back("DeathLink");
            ap->ConnectUpdate(0b111, tags);
        }

        apStone.CreateItem();
        SaveToDatabase();
        SyncLocationChecks();
        ap->GetDataPackage(ap->GetAllGames());
    }

    void AP_Character::APSocketErrorHandler(const std::string& error)
    {
        LOG_ERROR("module.archipelawow", "Archipelago socket error: {}", error);
    }

    void AP_Character::APSocketDisconnectedHandler()
    {
        LOG_WARN("module.archipelawow", "Disconnected from Archipelago server");
        ChatHandler(player->GetSession()).SendSysMessage("|cFFFF0000Disconnected from Archipelago server");
    }

    void AP_Character::APBouncedHandler(const nlohmann::json& packet)
    {
        if (!packet.contains("tags") || !packet["tags"].is_array())
        {
            return;
        }

        std::list<std::string> tags = packet["tags"];

        bool deathlink = (std::find(tags.begin(), tags.end(), "DeathLink") != tags.end());
        if (deathlink && deathLinkEnabled)
        {
            if (!packet.contains("data") || !packet["data"].is_object())
            {
                return;
            }

            const auto& data = packet["data"];
            std::string cause = data.value("cause", "");
            std::string source = data.value("source", "");

            // DeathLink bounces come from other games' implementations; some
            // omit the timestamp, so fall back to "now".
            double timestamp = data.contains("time") && data["time"].is_number()
                ? data["time"].get<double>()
                : ap->GetServerTime();

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

        // Fetch our own and the server's data package before connecting the
        // slot, so the initial item/location messages render proper names.
        // The remaining games are fetched after the slot is connected.
        ap->GetDataPackage({ AP_GAME_NAME, "Archipelago" });
    }

    void AP_Character::APDataPackageHandler(const nlohmann::json& data)
    {
        if (!ap)
        {
            return;
        }

        size_t games = data.contains("games") ? data["games"].size() : 0;
        LOG_INFO("module.archipelawow", "Data package updated ({} games)", games);

        if (ap->GetState() < Network::Client::State::SlotConnected)
        {
            ConnectAPSlot();
        }
    }

    void AP_Character::APReceivedItemsHandler(const std::list<Network::Client::NetworkItem>& items)
    {
        LOG_DEBUG("module.archipelawow", "Received {} items", items.size());

        int newItemIndex = itemIndex;
        int newCountedItemIndex = countedItemIndex;
        uint32 movementSpeedBefore = GetProgressiveStep(Items::ProgressiveType::MovementSpeed);

        for (const auto& item : items)
        {
            bool alreadyRewarded = itemIndex >= item.index;
            // countedItemIndex, unlike itemIndex, is not persisted: progressive bonuses live only
            // in memory and are rebuilt from the item list the server replays on every connection.
            bool alreadyCounted = countedItemIndex >= item.index;
            int sender = item.player;

            if (item.index > newItemIndex)
            {
                newItemIndex = item.index;
            }
            if (item.index > newCountedItemIndex)
            {
                newCountedItemIndex = item.index;
            }

            LOG_DEBUG("module.archipelawow", "Received item: index {}, sender {}, item ID {}, flags {}, already rewarded {}",
                item.index, sender, item.item, item.flags, alreadyRewarded);
            RewardItem(item.item, alreadyRewarded, alreadyCounted, sender);
        }

        countedItemIndex = newCountedItemIndex;

        if (GetProgressiveStep(Items::ProgressiveType::MovementSpeed) != movementSpeedBefore)
        {
            ApplyMovementSpeedBonus();
        }

        // The safety net behind OnPlayerAfterTakeItemFromMail, which does the tidying up as tiers are
        // actually collected. This catches anything that hook missed -- items collected while the
        // slot was disconnected, or a tier that arrived before the module was installed -- and
        // costs nothing when there is nothing to sweep.
        RemoveOutgrownProgressiveItems(Items::ProgressiveType::Food);
        RemoveOutgrownProgressiveItems(Items::ProgressiveType::Drink);

        // Runs after the loop above, so every spell the multiworld has already handed over has been
        // recorded and only the ones the character was never meant to have are taken back.
        RemoveUngrantedSpells();

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

        LOG_ERROR("module.archipelawow", "Connection to slot {} refused: {}", slot, joined);
        ChatHandler(player->GetSession()).SendSysMessage(fmt::format("|cFFFF0000Connection to slot |cFFFF00FF{}|cFFFF0000 refused: {}|r", slot, joined));

        run = false;
    }

    void AP_Character::APMessageErrorHandler(const std::string& error)
    {
        LOG_ERROR("module.archipelawow", "APClient message processing error: {}", error);
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
        return boost::uuids::to_string(boost::uuids::random_generator()());
    }
}
