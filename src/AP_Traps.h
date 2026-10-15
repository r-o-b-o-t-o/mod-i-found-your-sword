#ifndef _MOD_ARCHIPELAWOW_AP_TRAPS_H_
#define _MOD_ARCHIPELAWOW_AP_TRAPS_H_

#include "Define.h"
#include "Duration.h"
#include "items/AP_Traps.h"
#include "ObjectGuid.h"
#include "Optional.h"
#include "Player.h"
#include "SpellInfo.h"

#include <string>
#include <vector>

namespace ModArchipelaWoW::Traps
{
    struct ZoneMap
    {
        uint32 zoneId;
        uint32 mapId;
    };

    struct FallingBomb
    {
        ObjectGuid bomb;
        /// The trigger on the landing spot, wearing the fire that shows the blast's reach
        ObjectGuid marker;
        Milliseconds markedAt;
        bool marked;
        Milliseconds explodesAt;
    };

    /// A Bombs Trap going off.
    struct BombRun
    {
        uint32 bombsLeft;
        Milliseconds nextDrop;
        /// Those on their way down or lying on the ground
        std::vector<FallingBomb> bombs;
    };

    struct TeleportSpot
    {
        uint32 zoneId;
        uint32 mapId;
        float x;
        float y;
        float z;
        uint8 level;
    };

    std::string GetName(Items::TrapType type);
    /// Keeps the trap auras from being dispelled, cleansed, broken by immunities or resisted early.
    /// Called for every spell as the core loads them.
    void FixSpellInfo(SpellInfo* spellInfo);
    /// Whether the trap can spring now. One that cannot waits, and is tried again on every update.
    bool CanSpring(Player* player, Items::TrapType type);
    /// Dazed, Shackle, Sheep and Poison.
    bool ApplyAura(Player* player, Items::TrapType type);
    /// A spot out in the wild of one of the zones, preferring ones whose creatures are no higher
    /// than a few levels above the player.
    Optional<TeleportSpot> FindTeleportSpot(Player* player, const std::vector<ZoneMap>& zones);
    void SpawnAmbush(Player* player);
    /// Moves the equipped weapons to the bags, those that fit. Returns how many were moved.
    uint32 DropWeapons(Player* player);
    BombRun StartBombRun();
    /// Drops and sets off the bombs as their time comes. Returns false once the run is over, or
    /// cut short by the character dying or teleporting.
    bool UpdateBombRun(Player* player, BombRun& run);
    /// Gives the ambushers being summoned the player's level.
    void OnBeforeCreatureSelectLevel(uint8& level);
}

#endif
