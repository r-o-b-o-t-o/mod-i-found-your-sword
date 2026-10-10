SET @apStoneId := 32618;
SET @apStoneName := 'Archipelago Stone';

DELETE FROM `item_template` WHERE `entry` = @apStoneId;

INSERT INTO `item_template` (`entry`, `name`, `displayid`, `class`, `quality`, `Material`, `ScriptName`, `stackable`, `spellid_1`, `maxcount`)
VALUES (@apStoneId, @apStoneName, 45112, 12, 1, 3, 'item_archipelawow_ap_stone', 1, 36177, 1);

UPDATE `item_template_locale` SET `Name` = @apStoneName WHERE (`ID` = @apStoneId);
