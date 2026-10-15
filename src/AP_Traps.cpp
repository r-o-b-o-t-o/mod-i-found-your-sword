#include "AP_Traps.h"
#include "CellImpl.h"
#include "Containers.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "CreatureData.h"
#include "DBCStores.h"
#include "DBCStructure.h"
#include "Define.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Item.h"
#include "ItemTemplate.h"
#include "items/AP_Traps.h"
#include "Map.h"
#include "MapMgr.h"
#include "ModelIgnoreFlags.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Optional.h"
#include "Player.h"
#include "Position.h"
#include "SharedDefines.h"
#include "SpellAuraEffects.h"
#include "SpellAuras.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "TemporarySummon.h"
#include "Unit.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

// Spells no creature, item or script of the core casts, so making them undispellable in FixSpellInfo
// changes nothing else. Their tooltip and icon are the client's own.
constexpr uint32 DAZED_TRAP_SPELL_ID = 51372;   // Dazed, -50% movement speed
constexpr uint32 SHACKLE_TRAP_SPELL_ID = 38505; // Shackle, rooted
constexpr uint32 SHEEP_TRAP_SPELL_ID = 27760;   // Polymorph, breaks on damage like a mage's
constexpr uint32 POISON_TRAP_SPELL_ID = 17183;  // Poison, nature damage every 3 seconds
constexpr int32 DAZED_TRAP_DURATION = 30 * IN_MILLISECONDS;
constexpr int32 SHACKLE_TRAP_DURATION = 15 * IN_MILLISECONDS;
constexpr int32 SHEEP_TRAP_DURATION = 15 * IN_MILLISECONDS;
constexpr int32 POISON_TRAP_DURATION = 30 * IN_MILLISECONDS;
// Percentage of the character's maximum health per tick, so it keeps up with its level: 40% over the 10 ticks.
// The tooltip still shows the spell's own amount, as the 3.3.5 client is never sent the real one.
constexpr uint32 POISON_TRAP_TICK_PCT = 4;

// How far above the character's level the creatures around a Teleport Trap's spot may be
constexpr uint8 TELEPORT_LEVEL_MARGIN = 3;
// Spots to try before giving up, each needing its zone and water checked on the terrain
constexpr uint32 TELEPORT_ATTEMPTS = 20;

constexpr float AMBUSH_DISTANCE = 6.0f;
constexpr uint32 AMBUSH_DESPAWN_TIME = MINUTE * IN_MILLISECONDS;
// Below this level, two ambushers rather than three
constexpr uint8 AMBUSH_FULL_LEVEL = 10;
// How many of the ambushers met closest to the character's level one is drawn from
constexpr size_t AMBUSHER_CHOICES = 3;

// archipelawow_world_018_bomb.sql: a half-size Scourge Bomb
constexpr uint32 BOMB_CREATURE_ENTRY = 100500;
// World Trigger, invisible: one stands on the landing spot to wear the fire marking it, another shows
// the blast once the bomb is gone, as a visual is drawn on its caster and would vanish with the bomb.
constexpr uint32 BOMB_TRIGGER_CREATURE_ENTRY = 22515;
// Flamestrike Ground Aura, a patch of fire the marker wears
constexpr uint32 BOMB_MARKER_SPELL_ID = 74543;
// Bombing Run: Explosion Visual, a dummy that only shows the blast
constexpr uint32 BOMB_EXPLOSION_SPELL_ID = 40162;
constexpr uint32 BOMBS_PER_TRAP = 40;
// From where the bomb falls
constexpr float BOMB_DROP_HEIGHT = 8.0f;
// How far from where the character stands
constexpr float BOMBARDMENT_RADIUS = 15.0f;
// Every so many bombs, one lands right next to the character, so that standing still is no safer
// than running.
constexpr uint32 BOMB_AIMED_EVERY = 3;
constexpr float BOMB_AIMED_MIN_DISTANCE = 0.5f;
constexpr float BOMB_AIMED_MAX_DISTANCE = 1.5f;
// Both timed from the moment a bomb spawns: the next one may drop before the last has gone off.
constexpr Milliseconds BOMB_FUSE = 2000ms;
constexpr Milliseconds BOMB_INTERVAL = 250ms;
// From the moment the bomb spawns, before the fire shows where it lands
constexpr Milliseconds BOMB_MARKER_DELAY = 600ms;
constexpr float BOMB_RADIUS = 5.0f;
// Landing spots drawn before settling for one within BOMB_RADIUS of a bomb yet to go off
constexpr uint32 BOMB_PLACEMENT_ATTEMPTS = 10;
// The fire patch's radius on a unit of scale 1, measured in game across a 1.5 scale character: 5.0
// yards wide. The marker is scaled to make it as wide as the blast.
constexpr float BOMB_MARKER_RADIUS_AT_SCALE_1 = 5.0f / 2.0f / 1.5f;
// Intentionally make the visual effect 10% larger so the border isn't ambiguous.
constexpr float BOMB_MARKER_OVERSHOOT = 1.1f;
constexpr float BOMB_MARKER_SCALE = (BOMB_RADIUS * BOMB_MARKER_OVERSHOOT) / BOMB_MARKER_RADIUS_AT_SCALE_1;
// Percentage of the character's maximum health: you can survive one bomb, not two in a row.
constexpr uint32 BOMB_DAMAGE_PCT = 60;

namespace
{
    struct Ambusher
    {
        uint32 entry;
        uint8 level;
    };

    // Plain humanoid bandits and assassins, by the level they are met at, whose scripts do nothing
    // more than fight. They are summoned at the character's level whatever theirs.
    constexpr Ambusher AMBUSHERS[] = {
        { 116, 8 },    // Defias Bandit
        { 583, 8 },    // Defias Ambusher
        { 1981, 10 },  // Dark Iron Ambusher
        { 452, 16 },   // Riverpaw Bandit
        { 4025, 23 },  // Bloodfury Ambusher
        { 579, 24 },   // Shadowhide Assassin
        { 10758, 25 }, // Grimtotem Bandit
        { 12977, 34 }, // Kolkar Ambusher
        { 2246, 38 },  // Syndicate Assassin
        { 5618, 41 },  // Wastewander Bandit
        { 5623, 42 },  // Wastewander Assassin
        { 11678, 52 }, // Snowblind Ambusher
        { 14372, 56 }, // Winterfall Ambusher
        { 20019, 58 }, // Astralan Bandit
        { 18116, 62 }, // Daggerfen Assassin
        { 16760, 64 }, // Der'izu Bandit
        { 21982, 66 }, // Wyrmcult Ambusher
        { 20287, 67 }, // Zaxxis Ambusher
        { 30208, 78 }, // Stormforged Ambusher
    };

    // Set while SpawnAmbush summons, read by the level hook down the same call. Creatures of every
    // map pass through that hook on their own thread, and must not see it.
    thread_local uint8 ambushLevel = 0;

    // Built on first use, by zone
    std::unordered_map<uint32, std::vector<ModArchipelaWoW::Traps::TeleportSpot>> teleportSpots;

    /// Spawns of hostile, plain, walking creatures: they live out in the wild rather than in a town,
    /// a camp of elites or the air.
    bool IsWildSpawn(const CreatureData& data)
    {
        if (!(data.phaseMask & PHASEMASK_NORMAL))
        {
            return false;
        }

        const CreatureTemplate* creature = sObjectMgr->GetCreatureTemplate(data.id);
        if (!creature || creature->rank != CREATURE_ELITE_NORMAL || creature->npcflag != 0)
        {
            return false;
        }
        if (creature->flags_extra & (CREATURE_FLAG_EXTRA_TRIGGER | CREATURE_FLAG_EXTRA_CIVILIAN | CREATURE_FLAG_EXTRA_GUARD))
        {
            return false;
        }
        if (creature->unit_flags & (UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_NOT_SELECTABLE))
        {
            return false;
        }
        if (!creature->Movement.IsGroundAllowed() || creature->Movement.IsFlightAllowed())
        {
            return false;
        }

        const FactionTemplateEntry* faction = sFactionTemplateStore.LookupEntry(creature->faction);
        return faction && faction->IsHostileToPlayers();
    }

    const std::vector<ModArchipelaWoW::Traps::TeleportSpot>& GetTeleportSpots(uint32 zoneId, uint32 mapId)
    {
        auto it = teleportSpots.find(zoneId);
        if (it != teleportSpots.end())
        {
            return it->second;
        }

        // Within the zone's world map rectangle. It overlaps the neighbouring zones, which
        // FindTeleportSpot weeds out on the terrain: doing that here would load every grid of the map.
        std::vector<ModArchipelaWoW::Traps::TeleportSpot>& spots = teleportSpots[zoneId];
        for (const auto& [spawnId, data] : sObjectMgr->GetAllCreatureData())
        {
            if (data.mapid != mapId || !IsWildSpawn(data))
            {
                continue;
            }

            float zoneX = data.posX;
            float zoneY = data.posY;
            Map2ZoneCoordinates(zoneX, zoneY, zoneId);
            if (zoneX < 0.0f || zoneX > 100.0f || zoneY < 0.0f || zoneY > 100.0f)
            {
                continue;
            }

            uint8 level = sObjectMgr->GetCreatureTemplate(data.id)->maxlevel;
            spots.push_back({ zoneId, mapId, data.posX, data.posY, data.posZ, level });
        }

        return spots;
    }

    /// @param taken The landing spots of the bombs yet to go off, which this one keeps clear of
    Optional<ModArchipelaWoW::Traps::FallingBomb> DropBomb(Player* player, bool aimed, Milliseconds now, const std::vector<Position>& taken)
    {
        Position landing;
        for (uint32 attempt = 0; attempt < BOMB_PLACEMENT_ATTEMPTS; ++attempt)
        {
            // The others spread evenly over the bombarded disc
            float distance = aimed ? frand(BOMB_AIMED_MIN_DISTANCE, BOMB_AIMED_MAX_DISTANCE) : BOMBARDMENT_RADIUS * std::sqrt(rand_norm());
            landing = player->GetNearPosition(distance, frand(0.0f, 2.0f * float(M_PI)));
            if (std::ranges::none_of(taken, [&landing](const Position& spot) { return spot.GetExactDist(landing) < BOMB_RADIUS; }))
            {
                break;
            }
        }

        TempSummon* marker = player->SummonCreature(BOMB_TRIGGER_CREATURE_ENTRY, landing, TEMPSUMMON_TIMED_DESPAWN,
            (BOMB_FUSE + 5s).count());
        if (!marker)
        {
            return {};
        }
        marker->SetObjectScale(BOMB_MARKER_SCALE);

        // From the sky, unless a roof or a cave's ceiling is in the way
        float dropZ = landing.GetPositionZ();
        if (player->GetMap()->isInLineOfSight(landing.GetPositionX(), landing.GetPositionY(), dropZ + 2.0f,
            landing.GetPositionX(), landing.GetPositionY(), dropZ + BOMB_DROP_HEIGHT,
            player->GetPhaseMask(), LINEOFSIGHT_ALL_CHECKS, VMAP::ModelIgnoreFlags::Nothing))
        {
            dropZ += BOMB_DROP_HEIGHT;
        }

        ModArchipelaWoW::Traps::FallingBomb falling{ ObjectGuid::Empty, marker->GetGUID(), now + BOMB_MARKER_DELAY, false, now + BOMB_FUSE };
        if (TempSummon* bomb = player->SummonCreature(BOMB_CREATURE_ENTRY, landing.GetPositionX(), landing.GetPositionY(), dropZ,
            landing.GetOrientation(), TEMPSUMMON_TIMED_DESPAWN, (BOMB_FUSE + 5s).count()))
        {
            bomb->GetMotionMaster()->MoveFall(0, true);
            falling.bomb = bomb->GetGUID();
        }
        return falling;
    }

    /// The bombs yet to go off, when the run is cut short
    void DespawnBombs(Player* player, const ModArchipelaWoW::Traps::BombRun& run)
    {
        for (const ModArchipelaWoW::Traps::FallingBomb& falling : run.bombs)
        {
            for (ObjectGuid guid : { falling.bomb, falling.marker })
            {
                if (Creature* creature = ObjectAccessor::GetCreature(*player, guid))
                {
                    creature->DespawnOrUnsummon();
                }
            }
        }
    }

    void ExplodeBomb(Creature* marker, Creature* bomb)
    {
        // From a trigger of its own, so that the blast is not drawn at the marker's scale
        if (TempSummon* blast = marker->SummonCreature(BOMB_TRIGGER_CREATURE_ENTRY, marker->GetPosition(), TEMPSUMMON_TIMED_DESPAWN,
            5 * IN_MILLISECONDS))
        {
            blast->CastSpell(blast, BOMB_EXPLOSION_SPELL_ID, true);
        }

        // Every player caught in the blast, not only the one the trap was sent to
        std::vector<Player*> players;
        Acore::AnyPlayerInObjectRangeCheck check(marker, BOMB_RADIUS);
        Acore::PlayerListSearcher<Acore::AnyPlayerInObjectRangeCheck> searcher(marker, players, check);
        Cell::VisitObjects(marker, searcher, BOMB_RADIUS);

        // From the bomb when there is one, so that the character is killed by a Bomb rather than a
        // World Trigger
        Unit* attacker = bomb ? static_cast<Unit*>(bomb) : marker;
        const SpellInfo* spellInfo = sSpellMgr->GetSpellInfo(BOMB_EXPLOSION_SPELL_ID);
        for (Player* player : players)
        {
            // The search allows for the marker's size, which its scale makes large
            if (player->GetExactDist(marker) > BOMB_RADIUS)
            {
                continue;
            }

            uint32 damage = CalculatePct(player->GetMaxHealth(), BOMB_DAMAGE_PCT);
            attacker->SendSpellNonMeleeDamageLog(player, spellInfo, damage, SPELL_SCHOOL_MASK_FIRE, 0, 0, false, 0);
            Unit::DealDamage(attacker, player, damage, nullptr, SPELL_DIRECT_DAMAGE, SPELL_SCHOOL_MASK_FIRE, spellInfo, false);
        }

        marker->DespawnOrUnsummon();
        if (bomb)
        {
            bomb->DespawnOrUnsummon();
        }
    }
}

namespace ModArchipelaWoW::Traps
{
    std::string GetName(Items::TrapType type)
    {
        switch (type)
        {
            case Items::TrapType::Dazed: return "Dazed Trap";
            case Items::TrapType::Shackle: return "Shackle Trap";
            case Items::TrapType::Sheep: return "Sheep Trap";
            case Items::TrapType::Poison: return "Poison Trap";
            case Items::TrapType::Teleport: return "Teleport Trap";
            case Items::TrapType::Ambush: return "Ambush Trap";
            case Items::TrapType::Disarm: return "Disarming Trap";
            case Items::TrapType::Bombs: return "Bombs Trap";
        }
        return "Trap";
    }

    void FixSpellInfo(SpellInfo* spellInfo)
    {
        switch (spellInfo->Id)
        {
            case DAZED_TRAP_SPELL_ID:
            case SHACKLE_TRAP_SPELL_ID:
            case SHEEP_TRAP_SPELL_ID:
            case POISON_TRAP_SPELL_ID:
                break;
            default:
                return;
        }

        // Dispels go by dispel type; Hand of Freedom, Every Man for Himself, Tremor Totem and
        // shapeshifting by mechanic; Divine Shield, Ice Block and Cloak of Shadows purge whatever
        // lacks NO_IMMUNITIES.
        spellInfo->Dispel = DISPEL_NONE;
        spellInfo->Mechanic = MECHANIC_NONE;
        for (SpellEffectInfo& effect : spellInfo->_GetEffects())
        {
            effect.Mechanic = MECHANIC_NONE;
        }
        spellInfo->Attributes |= SPELL_ATTR0_NO_IMMUNITIES;
        spellInfo->Attributes &= ~SPELL_ATTR0_HEARTBEAT_RESIST;
    }

    bool CanSpring(Player* player, Items::TrapType type)
    {
        if (!player->IsInWorld() || !player->IsAlive() || player->IsBeingTeleported() || player->IsInFlight() || player->GetVehicle())
        {
            return false;
        }

        // A trap would decide the match
        if (player->GetMap()->IsBattlegroundOrArena())
        {
            return false;
        }

        switch (type)
        {
            case Items::TrapType::Sheep:
            case Items::TrapType::Ambush:
                // The sheep would fall off its flying mount, and the ambushers could not reach it.
                return !player->IsFlying();
            case Items::TrapType::Bombs:
                // A mount, even on the ground, would outrun them
                return !player->IsFlying() && !player->IsMounted();
            case Items::TrapType::Teleport:
                // Not out of a dungeon, which would cost the group its run, nor in the air, which the
                // stone's teleports refuse too: a jump's rise included, which Player::IsFalling misses.
                return !player->GetMap()->Instanceable() && !player->HasUnitMovementFlag(MOVEMENTFLAG_FALLING | MOVEMENTFLAG_FALLING_FAR);
            default:
                return true;
        }
    }

    bool ApplyAura(Player* player, Items::TrapType type)
    {
        uint32 spellId;
        int32 duration;
        switch (type)
        {
            case Items::TrapType::Dazed:
                spellId = DAZED_TRAP_SPELL_ID;
                duration = DAZED_TRAP_DURATION;
                break;
            case Items::TrapType::Shackle:
                spellId = SHACKLE_TRAP_SPELL_ID;
                duration = SHACKLE_TRAP_DURATION;
                break;
            case Items::TrapType::Sheep:
                spellId = SHEEP_TRAP_SPELL_ID;
                duration = SHEEP_TRAP_DURATION;
                break;
            case Items::TrapType::Poison:
                spellId = POISON_TRAP_SPELL_ID;
                duration = POISON_TRAP_DURATION;
                break;
            default:
                return false;
        }

        if (type == Items::TrapType::Sheep)
        {
            player->RemoveAurasByType(SPELL_AURA_MOUNTED);
        }

        // Added rather than cast, like the world buffs, so it lands on the character whatever the
        // spell targets.
        Aura* aura = player->AddAura(spellId, player);
        if (!aura)
        {
            return false;
        }

        aura->SetMaxDuration(duration);
        aura->SetDuration(duration);

        if (type == Items::TrapType::Poison)
        {
            if (AuraEffect* effect = aura->GetEffect(EFFECT_0))
            {
                effect->ChangeAmount(std::max<int32>(1, CalculatePct(player->GetMaxHealth(), POISON_TRAP_TICK_PCT)));
            }
        }

        return true;
    }

    Optional<TeleportSpot> FindTeleportSpot(Player* player, const std::vector<ZoneMap>& zones)
    {
        uint8 maxLevel = player->GetLevel() + TELEPORT_LEVEL_MARGIN;
        std::vector<const TeleportSpot*> nearLevel;
        std::vector<const TeleportSpot*> any;
        for (const ZoneMap& zone : zones)
        {
            for (const TeleportSpot& spot : GetTeleportSpots(zone.zoneId, zone.mapId))
            {
                any.push_back(&spot);
                if (spot.level <= maxLevel)
                {
                    nearLevel.push_back(&spot);
                }
            }
        }

        const std::vector<const TeleportSpot*>& candidates = nearLevel.empty() ? any : nearLevel;
        if (candidates.empty())
        {
            return {};
        }

        for (uint32 attempt = 0; attempt < TELEPORT_ATTEMPTS; ++attempt)
        {
            const TeleportSpot* spot = Acore::Containers::SelectRandomContainerElement(candidates);
            Map* map = sMapMgr->CreateBaseMap(spot->mapId);
            if (map->GetZoneId(PHASEMASK_NORMAL, spot->x, spot->y, spot->z) != spot->zoneId)
            {
                continue;
            }
            if (map->IsInWater(PHASEMASK_NORMAL, spot->x, spot->y, spot->z, player->GetCollisionHeight()))
            {
                continue;
            }
            return *spot;
        }

        return {};
    }

    void SpawnAmbush(Player* player)
    {
        uint8 level = player->GetLevel();

        // The few met closest under the character's level, or the lowest few for a character under them all
        auto end = std::ranges::find_if(AMBUSHERS, [level](const Ambusher& ambusher) { return ambusher.level > level; });
        size_t count = std::max(AMBUSHER_CHOICES, static_cast<size_t>(end - std::begin(AMBUSHERS)));
        size_t first = count > AMBUSHER_CHOICES ? count - AMBUSHER_CHOICES : 0;
        uint32 entry = AMBUSHERS[urand(first, count - 1)].entry;

        uint32 amount = level < AMBUSH_FULL_LEVEL ? 2 : 3;
        ambushLevel = level;
        for (uint32 i = 0; i < amount; ++i)
        {
            Position pos = player->GetNearPosition(AMBUSH_DISTANCE, 2.0f * float(M_PI) * i / amount);
            TempSummon* ambusher = player->SummonCreature(entry, pos, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, AMBUSH_DESPAWN_TIME);
            if (!ambusher)
            {
                continue;
            }

            // Their own factions are friendly to some players
            ambusher->SetFaction(FACTION_MONSTER);
            if (ambusher->AI())
            {
                ambusher->AI()->AttackStart(player);
            }
        }
        ambushLevel = 0;
    }

    uint32 DropWeapons(Player* player)
    {
        uint32 dropped = 0;
        for (uint8 slot : { EQUIPMENT_SLOT_MAINHAND, EQUIPMENT_SLOT_OFFHAND, EQUIPMENT_SLOT_RANGED })
        {
            // Shields, held-in-off-hand items and relics stay
            Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!item || item->GetTemplate()->Class != ITEM_CLASS_WEAPON)
            {
                continue;
            }

            ItemPosCountVec dest;
            if (player->CanUnequipItem(item->GetPos(), false) != EQUIP_ERR_OK ||
                player->CanStoreItem(NULL_BAG, NULL_SLOT, dest, item, false) != EQUIP_ERR_OK)
            {
                continue;
            }

            player->RemoveItem(INVENTORY_SLOT_BAG_0, slot, true);
            player->StoreItem(dest, item, true);
            ++dropped;
        }
        return dropped;
    }

    BombRun StartBombRun()
    {
        return { BOMBS_PER_TRAP, GameTime::GetGameTimeMS(), {} };
    }

    bool UpdateBombRun(Player* player, BombRun& run)
    {
        if (!player->IsInWorld() || !player->IsAlive() || player->IsBeingTeleported())
        {
            DespawnBombs(player, run);
            return false;
        }

        Milliseconds now = GameTime::GetGameTimeMS();
        for (FallingBomb& falling : run.bombs)
        {
            if (!falling.marked && now >= falling.markedAt)
            {
                if (Creature* marker = ObjectAccessor::GetCreature(*player, falling.marker))
                {
                    marker->AddAura(BOMB_MARKER_SPELL_ID, marker);
                }
                falling.marked = true;
            }
        }

        std::erase_if(run.bombs, [player, now](const FallingBomb& falling)
            {
                if (now < falling.explodesAt)
                {
                    return false;
                }

                // Missing once it went with the map the character left
                if (Creature* marker = ObjectAccessor::GetCreature(*player, falling.marker))
                {
                    ExplodeBomb(marker, ObjectAccessor::GetCreature(*player, falling.bomb));
                }
                return true;
            });

        // Killed by one of them
        if (!player->IsAlive())
        {
            DespawnBombs(player, run);
            return false;
        }

        if (run.bombsLeft > 0 && now >= run.nextDrop)
        {
            std::vector<Position> taken;
            for (const FallingBomb& falling : run.bombs)
            {
                if (Creature* marker = ObjectAccessor::GetCreature(*player, falling.marker))
                {
                    taken.push_back(marker->GetPosition());
                }
            }

            bool aimed = run.bombsLeft % BOMB_AIMED_EVERY == 0;
            if (Optional<FallingBomb> falling = DropBomb(player, aimed, now, taken))
            {
                run.bombs.push_back(falling.value());
            }
            --run.bombsLeft;
            run.nextDrop = now + BOMB_INTERVAL;
        }

        return run.bombsLeft > 0 || !run.bombs.empty();
    }

    void OnBeforeCreatureSelectLevel(uint8& level)
    {
        if (ambushLevel != 0)
        {
            level = ambushLevel;
        }
    }
}
