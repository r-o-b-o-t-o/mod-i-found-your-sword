-- The Scroll of Divination, an Archipelago filler item that hints 3 of the unchecked locations of the
-- character's world.
--
-- Like the Archipelago Stone, it reuses an item no player can obtain, since the client takes an item's
-- icon from its own Item.dbc: "Deprecated Scroll of Spirit Armor V", whose INV_Scroll_05 suits it.
-- Its class, subclass, material and display are Item.dbc's (see the README).
--
-- It shares the stone's use spell (archipelawow_world_009_stone_spell.sql), with a charge that uses
-- the scroll up once cast. The module refuses the use when there is nothing to hint, and applies the
-- effect as the spell is cast.

SET @scrollId := 4427;
SET @scrollName := 'Scroll of Divination';

DELETE FROM `item_template` WHERE `entry` = @scrollId;

INSERT INTO `item_template` (`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `Material`, `bonding`, `stackable`, `spellid_1`, `spellcharges_1`, `description`, `ScriptName`)
VALUES (@scrollId, 0, 8, @scrollName, 6270, 1, 7, 1, 20, 41024, -1, 'Reveals 3 unchecked locations of your world.', 'item_archipelawow_consumable');

DELETE FROM `item_template_locale` WHERE `ID` = @scrollId;
