-- The Enduring Soulstone, an Archipelago filler item that stores the soul of the character or of the
-- group member it targets for an hour: the warlock's Soulstone Resurrection, with the module stretching
-- its 15 minutes.
--
-- Built as the Scroll of Divination is (archipelawow_world_014_divination_scroll.sql), on
-- "NPC Equip 2767", a placeholder no creature equips, whose INV_Misc_Orb_04 is the soulstones' own.

SET @soulstoneId := 2767;
SET @soulstoneName := 'Enduring Soulstone';

DELETE FROM `item_template` WHERE `entry` = @soulstoneId;

INSERT INTO `item_template` (`entry`, `class`, `subclass`, `name`, `displayid`, `Quality`, `Material`, `bonding`, `stackable`, `spellid_1`, `spellcharges_1`, `description`, `ScriptName`)
VALUES (@soulstoneId, 12, 0, @soulstoneName, 1695, 1, -1, 1, 20, 41024, -1, 'Stores the soul of your targeted group member, or your own, for 1 hour. Upon death, they can come back to life where they fell.', 'item_archipelawow_consumable');

DELETE FROM `item_template_locale` WHERE `ID` = @soulstoneId;
