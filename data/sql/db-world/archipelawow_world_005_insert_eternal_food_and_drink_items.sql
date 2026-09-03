-- Eternal food and drink backing the "Progressive Food" and "Progressive Drink" Archipelago items.
--
-- Every entry below is a Blizzard placeholder row named "NPC Equip <id>": it exists only to carry
-- a display id, and nothing in the world database points at it. None appear in
-- creature_equip_template, npc_vendor, any loot table, or a reachable quest, so repurposing them
-- takes nothing away from a player or an NPC. (Item 37103 is listed as RequiredItemId1 on quest
-- 11934 "Unlocking Your Potential [PH]", a placeholder quest with no starter and no ender anywhere
-- in the database, so no character can ever accept it.)
--
-- The spells are the generic Food and Drink spells, one rung of the vendor ladder every ten levels
-- so each tier restores meaningfully more than the last. Food and drink line up rung for rung. The
-- client builds the "Restores N health over 20 sec" tooltip line out of its own Spell.dbc, so the
-- numbers stay correct without being repeated here.
--
--   tier | food spell (from)                      | drink spell (from)                    | req
--   -----|----------------------------------------|---------------------------------------|----
--    1   |   435 Bristle Whisker Catfish  ( 4593) |   432 Melon Juice             ( 1205) | 15
--    2   |  1129 Spotted Yellowtail       ( 6887) |  1135 Moonberry Juice         ( 1645) | 35
--    3   |  1131 Roasted Quail            ( 8952) |  1137 Morning Glory Dew       ( 8766) | 45
--    4   | 27094 Smoked Talbuk Venison    (27854) | 22734 Conjured Crystal Water  ( 8079) | 55
--    5   | 35270 Clefthoof Ribs           (29451) | 27089 Purified Draenic Water  (27860) | 65
--    6   | 45548 Sparkling Frostcap       (35947) | 43183 Honeymint Tea           (33445) | 75
--
-- Tiers 5 and 6 are the Burning Crusade and Wrath rungs. The apworld leaves them out of the item
-- pool when the seed's goal stops short of that content, so a level 60 seed never hands them out --
-- but the rows are still defined here, because the same module serves every seed on the realm.
--
-- What makes them eternal is `spellcharges_1` = 0. Both Spell::CheckItems and Spell::TakeCastItem
-- guard their charge handling behind a non-zero SpellCharges, so a zero-charge consumable is never
-- counted down and never used up.
--
-- These are UPDATEs rather than DELETE + INSERT on purpose. With DBC.EnforceItemAttributes = 1 --
-- the shipped default -- ObjectMgr::LoadItemTemplates overwrites class, subclass,
-- SoundOverrideSubclass, Material, InventoryType, displayid and sheath with the values out of
-- Item.dbc, and logs an error for every mismatch. Leaving those columns alone keeps the rows in
-- step with the client's own Item.dbc. What does matter is that all twelve have InventoryType 0,
-- because WorldSession::HandleUseItemOpcode refuses to use an equippable item that is not
-- equipped.
--
-- Shared column choices:
--   Quality 2         uncommon, so they stand out against the white stacks of ordinary food
--   Flags 32          ITEM_FLAG_NO_USER_DESTROY, so a slip of the mouse cannot bin an eternal
--                     item. Only the client-side destroy handler honours it, so the module can
--                     still clear out a tier the character has outgrown
--   bonding 1         bind on pickup; with no buy or sell price they cannot be traded away
--   maxcount 1        one is all a character can hold, and all it ever needs
--   RequiredLevel 0   the Archipelago progression is the gate, not the character level
--   spellcategory     11 (Food) or 59 (Drink), keeping the shared 1s category cooldown
--   FoodType 0        not pet food; an infinite pet feeder is not the intent here

-- Progressive Food
UPDATE `item_template` SET `name` = 'The Immortal Crust', `description` = 'Three innkeepers have tried. The crust remains.',
    `Quality` = 2, `Flags` = 32, `BuyPrice` = 0, `SellPrice` = 0, `ItemLevel` = 25, `RequiredLevel` = 0, `maxcount` = 1, `stackable` = 1, `bonding` = 1,
    `spellid_1` = 435, `spelltrigger_1` = 0, `spellcharges_1` = 0, `spellppmRate_1` = -1, `spellcooldown_1` = 0, `spellcategory_1` = 11, `spellcategorycooldown_1` = 1000, `FoodType` = 0
    WHERE `entry` = 19063;
UPDATE `item_template` SET `name` = 'Suspiciously Regrowing Berries', `description` = 'Plucked low from a startled Ancient. By morning it had grown a new pair.',
    `Quality` = 2, `Flags` = 32, `BuyPrice` = 0, `SellPrice` = 0, `ItemLevel` = 45, `RequiredLevel` = 0, `maxcount` = 1, `stackable` = 1, `bonding` = 1,
    `spellid_1` = 1129, `spelltrigger_1` = 0, `spellcharges_1` = 0, `spellppmRate_1` = -1, `spellcooldown_1` = 0, `spellcategory_1` = 11, `spellcategorycooldown_1` = 1000, `FoodType` = 0
    WHERE `entry` = 40843;
UPDATE `item_template` SET `name` = 'Fillet of Neverfin', `description` = 'Not the catch of the day. The catch of every day.',
    `Quality` = 2, `Flags` = 32, `BuyPrice` = 0, `SellPrice` = 0, `ItemLevel` = 55, `RequiredLevel` = 0, `maxcount` = 1, `stackable` = 1, `bonding` = 1,
    `spellid_1` = 1131, `spelltrigger_1` = 0, `spellcharges_1` = 0, `spellppmRate_1` = -1, `spellcooldown_1` = 0, `spellcategory_1` = 11, `spellcategorycooldown_1` = 1000, `FoodType` = 0
    WHERE `entry` = 34770;
UPDATE `item_template` SET `name` = 'Helboar Shank of Infinite Regret', `description` = 'You will finish it. You will regret it. You will finish it again tomorrow.',
    `Quality` = 2, `Flags` = 32, `BuyPrice` = 0, `SellPrice` = 0, `ItemLevel` = 65, `RequiredLevel` = 0, `maxcount` = 1, `stackable` = 1, `bonding` = 1,
    `spellid_1` = 27094, `spelltrigger_1` = 0, `spellcharges_1` = 0, `spellppmRate_1` = -1, `spellcooldown_1` = 0, `spellcategory_1` = 11, `spellcategorycooldown_1` = 1000, `FoodType` = 0
    WHERE `entry` = 35710;
UPDATE `item_template` SET `name` = 'Cinderfowl Drumstick', `description` = 'Somewhere, a very patient bird is growing another leg.',
    `Quality` = 2, `Flags` = 32, `BuyPrice` = 0, `SellPrice` = 0, `ItemLevel` = 75, `RequiredLevel` = 0, `maxcount` = 1, `stackable` = 1, `bonding` = 1,
    `spellid_1` = 35270, `spelltrigger_1` = 0, `spellcharges_1` = 0, `spellppmRate_1` = -1, `spellcooldown_1` = 0, `spellcategory_1` = 11, `spellcategorycooldown_1` = 1000, `FoodType` = 0
    WHERE `entry` = 43496;
UPDATE `item_template` SET `name` = 'Ribs of the Endless Banquet', `description` = 'Stolen from a feast that never ended. Nobody has noticed yet.',
    `Quality` = 2, `Flags` = 32, `BuyPrice` = 0, `SellPrice` = 0, `ItemLevel` = 85, `RequiredLevel` = 0, `maxcount` = 1, `stackable` = 1, `bonding` = 1,
    `spellid_1` = 45548, `spelltrigger_1` = 0, `spellcharges_1` = 0, `spellppmRate_1` = -1, `spellcooldown_1` = 0, `spellcategory_1` = 11, `spellcategorycooldown_1` = 1000, `FoodType` = 0
    WHERE `entry` = 36831;

-- Progressive Drink
UPDATE `item_template` SET `name` = 'Never-Empty Mug of Tavern Runoff', `description` = 'Scraped from every tankard in the inn, and it never runs out. Bottoms up.',
    `Quality` = 2, `Flags` = 32, `BuyPrice` = 0, `SellPrice` = 0, `ItemLevel` = 25, `RequiredLevel` = 0, `maxcount` = 1, `stackable` = 1, `bonding` = 1,
    `spellid_1` = 432, `spelltrigger_1` = 0, `spellcharges_1` = 0, `spellppmRate_1` = -1, `spellcooldown_1` = 0, `spellcategory_1` = 59, `spellcategorycooldown_1` = 1000, `FoodType` = 0
    WHERE `entry` = 33062;
UPDATE `item_template` SET `name` = 'Basin of Lesser Miracles', `description` = 'Every miracle it has worked so far has been mild.',
    `Quality` = 2, `Flags` = 32, `BuyPrice` = 0, `SellPrice` = 0, `ItemLevel` = 45, `RequiredLevel` = 0, `maxcount` = 1, `stackable` = 1, `bonding` = 1,
    `spellid_1` = 1135, `spelltrigger_1` = 0, `spellcharges_1` = 0, `spellppmRate_1` = -1, `spellcooldown_1` = 0, `spellcategory_1` = 59, `spellcategorycooldown_1` = 1000, `FoodType` = 0
    WHERE `entry` = 37103;
UPDATE `item_template` SET `name` = 'Bottomless Bottle of Dubious Vintage', `description` = 'The year on the label keeps changing. So does the taste.',
    `Quality` = 2, `Flags` = 32, `BuyPrice` = 0, `SellPrice` = 0, `ItemLevel` = 55, `RequiredLevel` = 0, `maxcount` = 1, `stackable` = 1, `bonding` = 1,
    `spellid_1` = 1137, `spelltrigger_1` = 0, `spellcharges_1` = 0, `spellppmRate_1` = -1, `spellcooldown_1` = 0, `spellcategory_1` = 59, `spellcategorycooldown_1` = 1000, `FoodType` = 0
    WHERE `entry` = 23704;
UPDATE `item_template` SET `name` = 'Perpetually Overflowing Tankard', `description` = 'Half of it is on the floor. There is always more.',
    `Quality` = 2, `Flags` = 32, `BuyPrice` = 0, `SellPrice` = 0, `ItemLevel` = 65, `RequiredLevel` = 0, `maxcount` = 1, `stackable` = 1, `bonding` = 1,
    `spellid_1` = 22734, `spelltrigger_1` = 0, `spellcharges_1` = 0, `spellppmRate_1` = -1, `spellcooldown_1` = 0, `spellcategory_1` = 59, `spellcategorycooldown_1` = 1000, `FoodType` = 0
    WHERE `entry` = 32913;
UPDATE `item_template` SET `name` = 'Netherbloom Nectar', `description` = 'Harvested where the ley lines leak. The glow is not a garnish.',
    `Quality` = 2, `Flags` = 32, `BuyPrice` = 0, `SellPrice` = 0, `ItemLevel` = 75, `RequiredLevel` = 0, `maxcount` = 1, `stackable` = 1, `bonding` = 1,
    `spellid_1` = 27089, `spelltrigger_1` = 0, `spellcharges_1` = 0, `spellppmRate_1` = -1, `spellcooldown_1` = 0, `spellcategory_1` = 59, `spellcategorycooldown_1` = 1000, `FoodType` = 0
    WHERE `entry` = 41374;
UPDATE `item_template` SET `name` = 'Elixir of the Everlasting Toast', `description` = 'To what? To everything. Again. Forever.',
    `Quality` = 2, `Flags` = 32, `BuyPrice` = 0, `SellPrice` = 0, `ItemLevel` = 85, `RequiredLevel` = 0, `maxcount` = 1, `stackable` = 1, `bonding` = 1,
    `spellid_1` = 43183, `spelltrigger_1` = 0, `spellcharges_1` = 0, `spellppmRate_1` = -1, `spellcooldown_1` = 0, `spellcategory_1` = 59, `spellcategorycooldown_1` = 1000, `FoodType` = 0
    WHERE `entry` = 42548;

-- Blizzard left names behind on some of these rows in other locales ("Eisbeeren", "Alabastertinte",
-- "Immersangportwein", ...). Dropping the locale rows rather than translating them makes every
-- client fall back to the names above, the way the module is English everywhere else.
DELETE FROM `item_template_locale` WHERE `ID` IN (19063, 40843, 34770, 35710, 43496, 36831, 33062, 37103, 23704, 32913, 41374, 42548);
