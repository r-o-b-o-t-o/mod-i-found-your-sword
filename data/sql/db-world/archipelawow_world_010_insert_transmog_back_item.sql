-- "Back" entry the Archipelago Stone adds to mod-transmog's vendor interface. The client does not
-- tell the server when a merchant window closes, so buying this entry is how the player returns to
-- the transmogrification menu. It is never handed out: the stone takes its purchase over, and when
-- the core handles it instead, it finds no vendor to buy it from.
--
-- A new row rather than a repurposed one, like mod-transmog's own "Hide Equipped" and "Clear
-- Transmog" entries. Nothing refers to it on a realm without mod-transmog.

SET @transmogBackId := 100500;
SET @transmogBackName := 'Back';

DELETE FROM `item_template` WHERE `entry` = @transmogBackId;

INSERT INTO `item_template` (`entry`, `class`, `subclass`, `name`, `displayid`, `quality`, `description`)
VALUES (@transmogBackId, 15, 0, @transmogBackName, 1322, 1, 'Return to the transmogrification menu.');

DELETE FROM `item_template_locale` WHERE `ID` = @transmogBackId;
