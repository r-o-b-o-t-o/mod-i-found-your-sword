# CLAUDE.md

AzerothCore module that turns a WotLK 3.3.5a realm into an Archipelago client. It lives in the
`modules/` directory of an AzerothCore checkout and is built as part of it. This directory is its own
git repository: run git commands from here, the core's git does not see it.

## Related repositories

- `archipelawow` — the APWorld that generates the seeds this module plays. Item and location names
  and IDs come from the data package it builds, and every key of its `fill_slot_data()` is parsed by
  `src/network/AP_Client.cpp` here. A change to any of those on one side needs a matching change on
  the other.
- `archipelawow-data-extractor` — generates the APWorld's `data/quests.json` and `data/spells.json`
  from an AzerothCore world database.

## Code

- Write straightforward code. Skip minor edge cases (ones only reachable with malformed data or
  config); point out notable ones (reachable in normal play, or that would crash the server or corrupt
  a save) and let the user decide whether they are worth handling.
- Comment only when the code isn't obvious or there is an implication a future maintainer could easily
  miss. Keep comments brief and don't restate the code.
- C++ follows the surrounding code, not the core's guidelines: `const X&` (not `X const&`), 4-space
  indent, LF, no trailing whitespace, no tabs.

## Working from the core checkout

The core's `AGENTS.md` loads alongside this file. Two of its rules do not apply here:

- Its build ban: an incremental compile of the `modules` target is cheap and is how to check a change
  compiles (only out-of-date translation units rebuild). Adding a new source file needs a CMake re-run
  first, since sources are globbed.
- Its SQL rule (`pending_db_*` only): the module has its own layout, see below.

`apps/ci/ci-codestyle.sh` needs GNU `grep -P`; under Git Bash it silently passes. It only checks
trailing whitespace, tabs, `LOG_*` with `GetCounter`, and triple newlines.

## SQL

- `data/sql/base/db_archipelawow/` — base schema of the module's own database. Do not edit; a schema
  or data change goes in a new file in `data/sql/updates/db_archipelawow/`, applied by worldserver on
  startup.
- `data/sql/db-world/` — changes to the core world database, applied by the core updater. Add a new
  `archipelawow_world_NNN_<description>.sql`; never edit an existing file.
- `data/sql/migration/` — one-off scripts users run by hand.

## Commits

- Conventional Commits with concise messages; one logical change per commit.
