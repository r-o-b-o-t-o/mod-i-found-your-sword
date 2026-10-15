#include "AP_Log.h"
#include "Containers.h"
#include "DatabaseEnv.h"
#include "DBCStore.h"
#include "DBCStores.h"
#include "DisableMgr.h"
#include "Define.h"
#include "items/AP_Gear.h"
#include "items/AP_GearPool.h"
#include "ItemTemplate.h"
#include "ObjectMgr.h"
#include "Optional.h"
#include "Player.h"
#include "QuestDef.h"
#include "SharedDefines.h"
#include "UnitDefines.h"
#include "World.h"
#include "WorldConfig.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace
{
    /// SkillRaceClassInfo.dbc, read for the one column AzerothCore leaves out.
    ///
    /// The core loads this file already, but its format string masks off MinLevel -- the level at
    /// which a race and class may first train the skill -- and that is the only column of interest
    /// here, so the file is read a second time rather than the core's parsing changed.
    struct SkillRaceClassInfoLevelEntry
    {
        // uint32 ID;                   sorted on, not stored
        uint32 SkillID;
        uint32 RaceMask;
        uint32 ClassMask;
        // uint32 Flags;                skipped
        uint32 MinLevel;
        // uint32 SkillTierID;          skipped
        // uint32 SkillCostIndex;       skipped
    };

    char constexpr SkillRaceClassInfoLevelFormat[] = "diiixixx";

    /// The four armor types a proficiency is trained for, weakest first.
    struct ArmorTier
    {
        uint32 subClass;
        uint32 skill;
    };

    constexpr ArmorTier ARMOR_TIERS[] =
    {
        { ITEM_SUBCLASS_ARMOR_CLOTH,   SKILL_CLOTH },
        { ITEM_SUBCLASS_ARMOR_LEATHER, SKILL_LEATHER },
        { ITEM_SUBCLASS_ARMOR_MAIL,    SKILL_MAIL },
        { ITEM_SUBCLASS_ARMOR_PLATE,   SKILL_PLATE_MAIL }
    };

    constexpr uint32 ALL_ARMOR_TIERS =
        (1u << ITEM_SUBCLASS_ARMOR_CLOTH) | (1u << ITEM_SUBCLASS_ARMOR_LEATHER) |
        (1u << ITEM_SUBCLASS_ARMOR_MAIL) | (1u << ITEM_SUBCLASS_ARMOR_PLATE);
}

namespace ModArchipelaWoW::Items
{
    GearPool::GearPool() :
        buckets(),
        skillTrainings(),
        maxLevel(0)
    {
    }

    void GearPool::Build()
    {
        maxLevel = static_cast<uint8>(sWorld->getIntConfig(CONFIG_MAX_PLAYER_LEVEL));

        for (auto& qualities : buckets)
        {
            for (auto& levels : qualities)
            {
                levels.assign(static_cast<std::size_t>(maxLevel) + 1, LevelBucket());
            }
        }

        LoadSkillTrainings();

        std::unordered_set<uint32> obtainable = LoadObtainableItems();
        std::unordered_map<uint32, uint8> questRewardLevels = LoadQuestRewardLevels();
        uint32 count = 0;

        for (ItemTemplate const* item : *sObjectMgr->GetItemTemplateStoreFast())
        {
            if (!item)
            {
                continue;
            }

            Optional<std::size_t> quality = QualityIndex(item->Quality);
            if (!quality)
            {
                continue;
            }

            // Level 0 and level 1 both mean "no meaningful requirement" in the item files, so what
            // they collect is the gear whose real gate is a quest or an event rather than a level.
            // Nearly every quest reward is one of them, and those get their quest's level instead;
            // the rest say nothing about when the piece is meant to be had. One asking for more than
            // the level cap can never be worn at all.
            uint32 level = item->RequiredLevel;
            if (level <= 1)
            {
                auto questLevel = questRewardLevels.find(item->ItemId);
                if (questLevel != questRewardLevels.end())
                {
                    level = questLevel->second;
                }
            }

            if (level <= 1 || level > maxLevel)
            {
                continue;
            }

            // Anything gated behind something a character cannot be assumed to have. A profession
            // rank, a specific spell, a reputation or an honor rank all leave the reward sitting
            // unequippable in the bags no matter how well it otherwise matches.
            if (item->RequiredSkill != 0 || item->RequiredSpell != 0 ||
                item->RequiredReputationFaction != 0 || item->RequiredHonorRank != 0)
            {
                continue;
            }

            Optional<GearCategory> category = CategoryOf(item);
            if (!category)
            {
                continue;
            }

            // The item templates carry Blizzard's internal test and deprecated entries -- "2100 Test
            // 2h Axe 63 blue", "Woven Ivy Necklace DEPRECATED", "Creature - Maiev's Glaive" -- right
            // alongside the real ones, and nothing in the template itself tells them apart. Asking
            // that the item be reachable somewhere in the world sorts them out without having to
            // guess from names.
            if (!obtainable.contains(item->ItemId))
            {
                continue;
            }

            buckets[static_cast<std::size_t>(*category)][*quality][level].push_back(item);
            ++count;
        }

        AP_LOG_INFO("Loaded {} gear reward candidates", count);
    }

    Optional<uint32> GearPool::Roll(GearCategory category, uint32 quality, Player const* player,
        uint8 levelWindow, bool allArmorTypes, bool heldOffHands, uint8 goalMaxLevel) const
    {
        Optional<std::size_t> qualityIndex = QualityIndex(quality);
        if (!player || !qualityIndex || category >= GearCategory::Count)
        {
            return {};
        }

        auto const& levels = buckets[static_cast<std::size_t>(category)][*qualityIndex];
        if (levels.empty())
        {
            return {};
        }

        // The run's own cap is the real ceiling, not the server's. A goal that ends at level 40
        // leaves a level 45 piece unwearable for good, and the window would happily reach one. A cap
        // of zero means the slot has not sent it yet, which cannot happen before an item arrives, so
        // it falls back to the server cap rather than refusing everything.
        int32 ceiling = maxLevel;
        if (goalMaxLevel > 0)
        {
            ceiling = std::min<int32>(ceiling, goalMaxLevel);
        }

        // Worked out from the configured window rather than the widened one: which armor types suit
        // the character is a decision about the reward, not a fallback for an empty draw.
        uint32 armorMask = allArmorTypes ? ALL_ARMOR_TIERS : AllowedArmorMask(player, levelWindow, ceiling);

        auto fits = [&](ItemTemplate const* item, int32 level)
        {
            return IsUsableBy(item, level, player) && MatchesArmorMask(item, armorMask) &&
                (heldOffHands || item->InventoryType != INVTYPE_HOLDABLE);
        };

        int32 playerLevel = std::min<int32>(player->GetLevel(), ceiling);
        int32 low = playerLevel - levelWindow;
        int32 high = playerLevel + levelWindow;

        // Widen the window until it holds a few things this character can wear. Overshooting is the
        // lesser evil -- a piece that has to be grown into still gets worn eventually, one that is
        // already ten levels stale never does -- and widening is the only way to serve a low level
        // character at all: no rare gear in the game asks for less than level 13, and no rare
        // jewelry for less than 17. The loop always ends: once the window covers the whole range
        // it stops whether or not it found the minimum.
        std::vector<ItemTemplate const*> matches;
        while (true)
        {
            matches.clear();

            int32 from = std::max<int32>(low, 1);
            int32 to = std::min<int32>(high, ceiling);
            for (int32 level = from; level <= to; ++level)
            {
                for (ItemTemplate const* item : levels[level])
                {
                    if (fits(item, level))
                    {
                        matches.push_back(item);
                    }
                }
            }

            if (matches.size() >= LEVEL_WINDOW_MIN_CANDIDATES || (low <= 1 && high >= ceiling))
            {
                break;
            }

            low -= LEVEL_WINDOW_WIDEN_STEP;
            high += LEVEL_WINDOW_WIDEN_STEP;
        }

        // Nothing at all under the run's cap -- no rare jewelry asks for less than level 17, so a
        // level 10 goal has none to give -- so the piece comes from the nearest level above it that
        // holds one, rather than the item being lost.
        for (int32 level = ceiling + 1; matches.empty() && level <= maxLevel; ++level)
        {
            for (ItemTemplate const* item : levels[level])
            {
                if (fits(item, level))
                {
                    matches.push_back(item);
                }
            }
        }

        if (matches.empty())
        {
            return {};
        }

        // Which weapon slots matter depends too much on the class for a slot roll to be fair.
        if (category == GearCategory::Weapon)
        {
            return Acore::Containers::SelectRandomContainerElement(matches)->ItemId;
        }

        // Armor and jewelry roll the slot first, or slots that hold more items would come up more
        // often: cloaks fit every armor type, so they far outnumber any other armor slot. A slot with
        // fewer than LEVEL_WINDOW_MIN_CANDIDATES items is weighted down in proportion, so the few
        // relics and trinkets in range don't keep handing out the same piece.
        std::map<uint32, std::vector<ItemTemplate const*>> slots;
        for (ItemTemplate const* item : matches)
        {
            slots[SlotOf(item)].push_back(item);
        }

        auto slot = Acore::Containers::SelectRandomWeightedContainerElement(slots, [](const auto& entry)
        {
            return static_cast<double>(SlotWeight(entry.first) *
                std::min(entry.second.size(), LEVEL_WINDOW_MIN_CANDIDATES));
        });

        return Acore::Containers::SelectRandomContainerElement(slot->second)->ItemId;
    }

    uint32 GearPool::SlotOf(ItemTemplate const* item)
    {
        return item->InventoryType == INVTYPE_ROBE ? INVTYPE_CHEST : item->InventoryType;
    }

    uint32 GearPool::SlotWeight(uint32 slot)
    {
        // Characters have two of each.
        return slot == INVTYPE_FINGER || slot == INVTYPE_TRINKET ? 2 : 1;
    }

    Optional<std::size_t> GearPool::QualityIndex(uint32 quality)
    {
        switch (quality)
        {
            case ITEM_QUALITY_UNCOMMON:
                return std::size_t(0);
            case ITEM_QUALITY_RARE:
                return std::size_t(1);
            default:
                return {};
        }
    }

    Optional<GearCategory> GearPool::CategoryOf(ItemTemplate const* item)
    {
        if (item->Class == ITEM_CLASS_WEAPON)
        {
            switch (item->SubClass)
            {
                // Fishing poles and the ITEM_SUBCLASS_WEAPON_MISC bucket -- blacksmith hammers,
                // mining picks, skinning knives, arclight spanners -- are weapons to the game files
                // but not to a player hoping for an upgrade. The obsolete, exotic and spear
                // subclasses hold nothing a 3.3.5a character can equip.
                case ITEM_SUBCLASS_WEAPON_obsolete:
                case ITEM_SUBCLASS_WEAPON_EXOTIC:
                case ITEM_SUBCLASS_WEAPON_EXOTIC2:
                case ITEM_SUBCLASS_WEAPON_MISC:
                case ITEM_SUBCLASS_WEAPON_SPEAR:
                case ITEM_SUBCLASS_WEAPON_FISHING_POLE:
                    return {};
                default:
                    return GearCategory::Weapon;
            }
        }

        if (item->Class != ITEM_CLASS_ARMOR)
        {
            return {};
        }

        switch (item->SubClass)
        {
            case ITEM_SUBCLASS_ARMOR_CLOTH:
            case ITEM_SUBCLASS_ARMOR_LEATHER:
            case ITEM_SUBCLASS_ARMOR_MAIL:
            case ITEM_SUBCLASS_ARMOR_PLATE:
                return GearCategory::Armor;
            case ITEM_SUBCLASS_ARMOR_BUCKLER:
            case ITEM_SUBCLASS_ARMOR_SHIELD:
                return GearCategory::Weapon;
            case ITEM_SUBCLASS_ARMOR_LIBRAM:
            case ITEM_SUBCLASS_ARMOR_IDOL:
            case ITEM_SUBCLASS_ARMOR_TOTEM:
            case ITEM_SUBCLASS_ARMOR_SIGIL:
                return GearCategory::Jewelry;
            case ITEM_SUBCLASS_ARMOR_MISC:
                break;
            default:
                return {};
        }

        // Miscellaneous armor is a grab bag. It holds the pieces every class wears without needing
        // any proficiency for them, and it also holds the shirts and tabards, which carry no stats
        // and would make for a flat reward.
        switch (item->InventoryType)
        {
            case INVTYPE_NECK:
            case INVTYPE_FINGER:
            case INVTYPE_TRINKET:
                return GearCategory::Jewelry;
            case INVTYPE_CLOAK:
                return GearCategory::Armor;
            case INVTYPE_HOLDABLE:
                return GearCategory::Weapon;
            default:
                return {};
        }
    }

    bool GearPool::IsUsableBy(ItemTemplate const* item, uint32 level, Player const* player) const
    {
        if ((item->AllowableClass & player->getClassMask()) == 0 ||
            (item->AllowableRace & player->getRaceMask()) == 0)
        {
            return false;
        }

        if (item->HasFlag2(ITEM_FLAG2_FACTION_HORDE) && player->GetTeamId(true) != TEAM_HORDE)
        {
            return false;
        }

        if (item->HasFlag2(ITEM_FLAG2_FACTION_ALLIANCE) && player->GetTeamId(true) != TEAM_ALLIANCE)
        {
            return false;
        }

        // Relics carry no proficiency skill, and AllowableClass is all fifteen classes on nearly
        // every one of them, so the class that can hold one is only ever implied by the subclass --
        // read here exactly the way Player::FindEquipSlot reads it.
        if (item->Class == ITEM_CLASS_ARMOR)
        {
            switch (item->SubClass)
            {
                case ITEM_SUBCLASS_ARMOR_LIBRAM:
                    return player->IsClass(CLASS_PALADIN, CLASS_CONTEXT_EQUIP_RELIC);
                case ITEM_SUBCLASS_ARMOR_IDOL:
                    return player->IsClass(CLASS_DRUID, CLASS_CONTEXT_EQUIP_RELIC);
                case ITEM_SUBCLASS_ARMOR_TOTEM:
                    return player->IsClass(CLASS_SHAMAN, CLASS_CONTEXT_EQUIP_RELIC);
                case ITEM_SUBCLASS_ARMOR_SIGIL:
                    return player->IsClass(CLASS_DEATH_KNIGHT, CLASS_CONTEXT_EQUIP_RELIC);
                default:
                    break;
            }
        }

        uint32 skill = item->GetSkill();
        if (skill == 0)
        {
            return true;
        }

        // Whether the class is allowed the skill at all.
        if (!GetSkillRaceClassInfo(skill, player->getRace(), player->getClass()))
        {
            return false;
        }

        // And whether it will have trained it by the time the piece can be worn. Mail and plate are
        // trained at level 40 and polearms at 20, so a hunter is only offered mail that asks for
        // level 40 or more -- gear it can equip the moment its own requirement is met, rather than
        // carry around for the twenty levels until the trainer will teach it.
        return level >= TrainingLevel(skill, player);
    }

    uint8 GearPool::TrainingLevel(uint32 skill, Player const* player) const
    {
        // First match wins on race then class, the same way GetSkillRaceClassInfo reads the file.
        for (SkillTraining const& training : skillTrainings)
        {
            if (training.skillId != skill)
            {
                continue;
            }

            if (training.raceMask && !(training.raceMask & player->getRaceMask()))
            {
                continue;
            }

            if (training.classMask && !(training.classMask & player->getClassMask()))
            {
                continue;
            }

            return training.minLevel;
        }

        return 0;
    }

    uint32 GearPool::AllowedArmorMask(Player const* player, uint8 levelWindow, int32 levelCeiling) const
    {
        int32 level = player->GetLevel();
        uint32 mask = 0;
        uint32 lastTraining = 0;
        bool kept = false;

        // Strongest type first. A type only counts as a step of its own when it is trained later
        // than the one above it: a warrior wears cloth, leather and mail from level one, so mail
        // already stands for all three and only plate at level 40 is a genuine step up.
        for (std::size_t i = std::size(ARMOR_TIERS); i-- > 0;)
        {
            ArmorTier const& tier = ARMOR_TIERS[i];
            if (!GetSkillRaceClassInfo(tier.skill, player->getRace(), player->getClass()))
            {
                continue;
            }

            uint32 training = TrainingLevel(tier.skill, player);

            // A type the run ends before the character could get any wear out of is not a step up at
            // all: a paladin whose goal stops at level 40 trains plate on the last level it will ever
            // have, so it stays on mail instead of collecting armor it can only put on at the finish.
            if (static_cast<int32>(training) >= levelCeiling)
            {
                continue;
            }

            if (kept && training >= lastTraining)
            {
                continue;
            }

            kept = true;
            lastTraining = training;

            // Offered as soon as the window reaches gear of that type, which is what lets a warrior
            // start collecting plate five levels before the trainer will teach it.
            if (level + levelWindow >= static_cast<int32>(training))
            {
                mask |= 1u << tier.subClass;
            }

            // And once this type is established rather than merely in reach, everything lighter has
            // been outgrown -- so the warrior keeps receiving mail until level 45, then stops.
            if (level > static_cast<int32>(training) + levelWindow)
            {
                break;
            }
        }

        return mask;
    }

    bool GearPool::MatchesArmorMask(ItemTemplate const* item, uint32 allowedMask)
    {
        // Only the four trained armor types are narrowed. Necks, rings and trinkets are
        // miscellaneous armor that every class wears whatever it is wearing elsewhere, and shields,
        // holdables and relics answer to their own rules.
        if (item->Class != ITEM_CLASS_ARMOR)
        {
            return true;
        }

        // Cloaks are filed under the cloth subclass -- every one of them in 3.3.5a -- but they need
        // no proficiency and every class wears one. Narrowing by armor type must not take them away
        // from a warrior along with the caster robes they share a subclass with.
        if (item->InventoryType == INVTYPE_CLOAK)
        {
            return true;
        }

        uint32 bit = 1u << item->SubClass;
        if (!(bit & ALL_ARMOR_TIERS))
        {
            return true;
        }

        return (bit & allowedMask) != 0;
    }

    void GearPool::LoadSkillTrainings()
    {
        skillTrainings.clear();

        DBCStorage<SkillRaceClassInfoLevelEntry> store(SkillRaceClassInfoLevelFormat);
        std::string path = sWorld->GetDataPath() + "dbc/SkillRaceClassInfo.dbc";
        if (!store.Load(path.c_str()))
        {
            AP_LOG_ERROR("Could not read {} -- gear rewards will ignore the level "
                "a class trains a proficiency at, and may hand out mail or plate too early", path);
            return;
        }

        for (SkillRaceClassInfoLevelEntry const* entry : store)
        {
            skillTrainings.push_back({ entry->SkillID, entry->RaceMask, entry->ClassMask,
                static_cast<uint8>(entry->MinLevel) });
        }
    }

    std::unordered_set<uint32> GearPool::LoadObtainableItems()
    {
        std::unordered_set<uint32> obtainable;

        // The outer filter is not just tidiness. A negative `npc_vendor`.`item` is a reference to
        // another vendor's list rather than an item id, and the unused reward slots of
        // `quest_template` sit at zero, so without it the result carries values that are not item
        // ids at all -- and Field::Get<uint32> logs a fatal on the negative ones.
        QueryResult result = WorldDatabase.Query(
            "SELECT `id` FROM ("
            "SELECT `Item` AS `id` FROM `creature_loot_template` "
            "UNION SELECT `Item` FROM `gameobject_loot_template` "
            "UNION SELECT `Item` FROM `item_loot_template` "
            "UNION SELECT `Item` FROM `reference_loot_template` "
            "UNION SELECT `Item` FROM `fishing_loot_template` "
            "UNION SELECT `Item` FROM `skinning_loot_template` "
            "UNION SELECT `Item` FROM `pickpocketing_loot_template` "
            "UNION SELECT `Item` FROM `mail_loot_template` "
            "UNION SELECT `Item` FROM `spell_loot_template` "
            "UNION SELECT `item` FROM `npc_vendor` "
            "UNION SELECT `RewardItem1` FROM `quest_template` "
            "UNION SELECT `RewardItem2` FROM `quest_template` "
            "UNION SELECT `RewardItem3` FROM `quest_template` "
            "UNION SELECT `RewardItem4` FROM `quest_template` "
            "UNION SELECT `RewardChoiceItemID1` FROM `quest_template` "
            "UNION SELECT `RewardChoiceItemID2` FROM `quest_template` "
            "UNION SELECT `RewardChoiceItemID3` FROM `quest_template` "
            "UNION SELECT `RewardChoiceItemID4` FROM `quest_template` "
            "UNION SELECT `RewardChoiceItemID5` FROM `quest_template` "
            "UNION SELECT `RewardChoiceItemID6` FROM `quest_template`"
            ") AS `sources` WHERE `id` > 0"
        );
        if (!result)
        {
            AP_LOG_WARN("Could not read the item sources used to filter gear rewards, "
                "test and deprecated items may be handed out");
            return obtainable;
        }

        do
        {
            obtainable.insert((*result)[0].Get<uint32>());
        } while (result->NextRow());

        return obtainable;
    }

    std::unordered_map<uint32, uint8> GearPool::LoadQuestRewardLevels()
    {
        std::unordered_map<uint32, uint8> levels;

        for (const auto& [questId, quest] : sObjectMgr->GetQuestTemplates())
        {
            // A quest level of -1 or less scales with the character, so it says nothing either.
            if (quest->GetQuestLevel() <= 0 || DisableMgr::IsDisabledFor(DISABLE_TYPE_QUEST, questId, nullptr))
            {
                continue;
            }

            // The same title heuristics the data extractor uses to drop dead quests.
            std::string title = quest->GetTitle();
            std::transform(title.begin(), title.end(), title.begin(), [](unsigned char c) { return std::tolower(c); });
            if (title.find('<') != std::string::npos || title.find('[') != std::string::npos ||
                title.find("deprecated") != std::string::npos || title.find("unused") != std::string::npos ||
                title.starts_with("temp "))
            {
                continue;
            }

            // The level the quest turns orange at rather than the one it can be picked up at, which is
            // when it is realistically done: Into The Temple of Atal'Hakkar is level 50 and can be
            // started at 38, so its Guardian Talisman counts as level 46.
            uint8 level = static_cast<uint8>(std::max<int32>(quest->GetQuestLevel() - QUEST_ORANGE_LEVEL_DIFF,
                static_cast<int32>(quest->GetMinLevel())));

            auto addReward = [&](uint32 itemId)
            {
                if (itemId == 0)
                {
                    return;
                }

                // An item several quests hand out counts from the earliest of them.
                auto [it, inserted] = levels.try_emplace(itemId, level);
                if (!inserted)
                {
                    it->second = std::min(it->second, level);
                }
            };

            for (uint32 itemId : quest->RewardItemId)
            {
                addReward(itemId);
            }

            for (uint32 itemId : quest->RewardChoiceItemId)
            {
                addReward(itemId);
            }
        }

        return levels;
    }
}
