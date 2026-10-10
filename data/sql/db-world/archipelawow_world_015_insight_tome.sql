-- The Tome of Insight, an Archipelago filler item that gives a quarter of the experience the next
-- Archipelago level needs.
--
-- Built as the Scroll of Divination is (archipelawow_world_014_divination_scroll.sql), on "Deprecated
-- Book of Nullify Disease", one of the old druid spell books, whose INV_Misc_Book_11 suits it. Item.dbc
-- files it as a recipe book, as it did every spell book.

SET @tomeId := 1335;
SET @tomeName := 'Tome of Insight';

DELETE FROM `item_template` WHERE `entry` = @tomeId;

INSERT INTO `item_template` (`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `Material`, `bonding`, `stackable`, `spellid_1`, `spellcharges_1`, `description`, `ScriptName`)
VALUES (@tomeId, 9, 0, @tomeName, 1317, 1, -1, 1, 20, 41024, -1, 'Grants a quarter of the experience needed for your next Archipelago level.', 'item_archipelawow_consumable');

DELETE FROM `item_template_locale` WHERE `ID` = @tomeId;
