# 🌍 ArchipelaWoW

This is a module for [AzerothCore](http://www.azerothcore.org) that adds support for [Archipelago](https://github.com/ArchipelagoMW/Archipelago) - a multi-game multi-world randomizer framework.

This repository contains the code for the [client](https://archipelago.miraheze.org/wiki/Client), you can find the [APWorld](https://archipelago.miraheze.org/wiki/APWorld) repository [here](https://github.com/r-o-b-o-t-o/archipelawow).

> ❔ What's with the weird module name?  
> It was originally named `mod-archipelawow`. However, I really wanted to integrate with `mod-autobalance` which brings experience scaling in dungeons. This module's XP gain hook conflicted with the one from AutoBalance, the only way to fix it was a rename (module hooks are fired in order of the module name). I wasn't able to come up with an interesting replacement name for the project as a whole that would come after `autobalance` so I decided to only rename the module with something that's reminiscent of multiplayer Archipelago games. Naming things is one of the hardest parts of software engineering, after all.

## 🚀 Installation

### Prerequisites

(*Optional*) The following modules are strongly recommended for seeds that include dungeon content (Dungeonmaster goals, dungeon quests option):
- [DungeonRespawn](https://github.com/Dreathean/DungeonRespawn)
- [mod-autobalance](https://github.com/azerothcore/mod-autobalance)
- [mod-solo-lfg](https://github.com/azerothcore/mod-solo-lfg)

### Setup guide

1. **Update AzerothCore**  
   Build against
   [`e1823bb`](https://github.com/azerothcore/azerothcore-wotlk/commit/e1823bb2db751a7cc0a90a8543e778449ebf7d84)
   (September 14th, 2026) or later:

   ```bash
   cd path/to/azerothcore-wotlk
   git pull
   ```

2. **Clone the repository** into the `modules` folder of your AzerothCore local clone
   ```bash
   cd path/to/azerothcore-wotlk/modules
   git clone https://github.com/r-o-b-o-t-o/mod-i-found-your-sword.git
   ```

3. **Re-run CMake**

4. **Build AzerothCore**

5. **Copy the configuration file**
   - Locate the configuration directory of your AzerothCore installation, usually `configs` for Windows or `etc` for Linux
   - In the `modules` subdirectory, copy `archipelawow.conf.dist` into `archipelawow.conf`

6. **Point the module to a database of its own**  
   `ArchipelaWoW.DatabaseInfo` in `archipelawow.conf` takes the same `host;port;user;password;database`
   string as the core's `*DatabaseInfo` entries, and defaults to `acore_archipelawow` on the local
   server. Like the core databases, it is populated and kept up to date by worldserver on startup
   while `ArchipelaWoW.Database.AutoUpdate` is on, and created when missing if `Updates.AutoSetup`
   is on too and the MySQL user may create databases.

   > 🔄 **Upgrading from a version that stored its tables in the core databases?**
   > Start worldserver once so the new database exists, then run
   > [`data/sql/migration/migrate_from_core_databases.sql`](data/sql/migration/migrate_from_core_databases.sql)
   > against your MySQL server to carry the slot bindings, level progress and location checks over.
   > The three schema names at the top of the file are the defaults; adjust them to your realm.

## 💬 Chat

A character bound to a slot shares that slot's seat in the multiworld, so the Archipelago room is
reachable from the game's own chat box - no need for a second client to alt-tab to.

### Talking to the room

What the character says in `/say` is sent to the room as chat from its slot, and everything the
room says back arrives as a system message, the same way item and hint announcements already do.
Set `ArchipelaWoW.Chat.RelaySay = 0` to disable relaying `/say` to the Archipelago room.

`ArchipelaWoW.Chat.RelayChannels` can also be used to relay chat messages. The default is a
channel named `archipelago`, which players can opt-in by typing `/join archipelago`, and
`/leave archipelago` to stop relaying.
Set `ArchipelaWoW.Chat.RelayChannels = ""` to disable relaying channels the the Archipelago room.

### Running Archipelago commands

A message starting with `!!` is handed to the Archipelago server as a command instead of being said
in game. It works in `/say` or any of the relayed channels.
Every command the server accepts is available:

| Command | What it does |
|---------|--------------|
| `!!hint Holy Light` | Buy a hint revealing where an item is |
| `!!hint_location Level 12` | Buy a hint revealing what a location holds |
| `!!status` | Show which slots are connected and how far along they are |
| `!!countdown 10` | Start a room-wide countdown, for a synchronized start |
| `!!players` | List the slots in the multiworld |
| `!!remaining`, `!!missing`, `!!checked` | Report on this slot's locations |
| `!!release`, `!!collect` | Send out this slot's remaining items, or claim its own |
| `!!alias Roboto` | Rename this slot in the room |
| `!!help` | The full list, straight from the server |

Archipelago's own marker is a single `!`, and the doubling is not decoration: AzerothCore takes
both `.` and `!` as prefixes for its own commands and parses them before any module is consulted,
so `!hint` would not work. The parser allows a doubled marker, so `!!` reaches the module,
which strips it and sends the Archipelago room the `!hint` it expects.

Set `ArchipelaWoW.Chat.CommandPrefix` to change the default `!!` marker.
Note that anything starting with a single `.` or `!` is swallowed by the core.

Set `ArchipelaWoW.Chat.CommandPrefix = ""` to disable sending commands to the Archipelago room.

## 🧩 Custom content

The module claims a small amount of space in shared game data. If you run other modules alongside
it, these are the values worth checking for conflicts - and the first place to look if an item or a
spell turns up with the wrong name, icon or behaviour.

### Spells

One serverside-only spell is added to `spell_dbc`. It is passive, so the aura is never sent to the
client and the player's `Spell.dbc` never needs to know about it.

| ID | Name | Purpose | Added by |
|----|------|---------|----------|
| `100500` | Archipelago Movement Speed | Carries the on-foot, ground-mount and flying-mount speed auras behind the *Progressive Movement Speed* item | `archipelawow_world_004_insert_movement_speed_spell.sql` |

### Items

No new `item_template` entries are created. Every item below reuses an existing row that no player
can obtain: either a `[DEPRECATED]` entry, or one of Blizzard's `NPC Equip <id>` placeholders, which
exist only to carry a display id. None of them appear in `creature_equip_template`, `npc_vendor`,
any loot table, or a reachable quest.

| ID | Repurposed as | Original entry | Added by |
|----|---------------|----------------|----------|
| `32618` | Archipelago Stone | `[DEPRECATED]Crystalforged Darkrune` | `archipelawow_world_001_insert_archipelago_stone_item.sql` |
| `19063` | The Immortal Crust | `NPC Equip 19063` | `archipelawow_world_005_insert_eternal_food_and_drink_items.sql` |
| `40843` | Suspiciously Regrowing Berries | `NPC Equip 40843` | `archipelawow_world_005_…` |
| `34770` | Fillet of Neverfin | `NPC Equip 34770` | `archipelawow_world_005_…` |
| `35710` | Helboar Shank of Infinite Regret | `NPC Equip 35710` | `archipelawow_world_005_…` |
| `43496` | Cinderfowl Drumstick | `NPC Equip 43496` | `archipelawow_world_005_…` |
| `36831` | Ribs of the Endless Banquet | `NPC Equip 36831` | `archipelawow_world_005_…` |
| `33062` | Never-Empty Mug of Tavern Runoff | `NPC Equip 33062` | `archipelawow_world_005_…` |
| `37103` | Basin of Lesser Miracles | `NPC Equip 37103` | `archipelawow_world_005_…` |
| `23704` | Bottomless Bottle of Dubious Vintage | `NPC Equip 23704` | `archipelawow_world_005_…` |
| `32913` | Perpetually Overflowing Tankard | `NPC Equip 32913` | `archipelawow_world_005_…` |
| `41374` | Netherbloom Nectar | `NPC Equip 41374` | `archipelawow_world_005_…` |
| `42548` | Elixir of the Everlasting Toast | `NPC Equip 42548` | `archipelawow_world_005_…` |

`19063` through `42548` are the six *Progressive Food* and six *Progressive Drink* tiers, in
progression order. The apworld leaves the fifth and sixth rungs out of seeds whose goal stops short
of The Burning Crusade and Wrath of the Lich King respectively, but the rows are defined here
regardless, since the same module serves every seed on the realm.

Their `class`, `subclass`, `SoundOverrideSubclass`, `Material`, `InventoryType`,
`displayid` and `sheath` are deliberately left at their original values: with
`DBC.EnforceItemAttributes = 1` (the AzerothCore default) `ObjectMgr::LoadItemTemplates` overwrites
those columns from `Item.dbc` and logs an error for every mismatch. Their `item_template_locale`
rows are deleted, so clients in every locale fall back to the English names above.

### Database objects

The module's own tables live in the database named by `ArchipelaWoW.DatabaseInfo`, built from
`data/sql/base/db_archipelawow/` and updated from `data/sql/updates/db_archipelawow/`. The rows below
are the only ones it adds to a core database.

| Object | Database | Added by |
|--------|----------|----------|
| `characters` | module | `base/db_archipelawow/characters.sql` |
| `location_check` | module | `base/db_archipelawow/location_check.sql` |
| `player_creature_template` | module | `base/db_archipelawow/player_creature_template.sql` |
| One `creature_template` row per Archipelago player that mails an item, entries `5150000` and up | world | at runtime, as mail senders |
| `.ap`, `.archipelago` and `.archipelawow` command prefixes | world (`command`) | `archipelawow_world_000_insert_commands.sql` |

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
