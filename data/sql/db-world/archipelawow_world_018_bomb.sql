-- The bomb the Bombs Trap drops on the character, wearing the Scourge Bomb's model at half its
-- size. It only falls and waits: the module sets it off, deals the damage and despawns it. Out of
-- reach of players and creatures (not attackable, immune to both, not selectable).

SET @bombEntry := 100500;
SET @bombDisplayId := 27462;

DELETE FROM `creature_template` WHERE `entry` = @bombEntry;
DELETE FROM `creature_template_model` WHERE `CreatureID` = @bombEntry;

INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `unit_class`, `unit_flags`, `type`, `AIName`)
VALUES (@bombEntry, 'Bomb', 1, 1, 35, 1, 33555202, 10, 'NullCreatureAI');

INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`)
VALUES (@bombEntry, 0, @bombDisplayId, 0.5, 1);
