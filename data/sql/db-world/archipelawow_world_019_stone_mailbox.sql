-- The mailbox each action in the Archipelago Stone's mailbox runs on. The module summons one at the
-- character's feet for each action, seen by no client, and despawns it right after.

SET @mailboxEntry := 100501;
SET @mailboxDisplayId := 11686;

DELETE FROM `creature_template` WHERE `entry` = @mailboxEntry;
DELETE FROM `creature_template_model` WHERE `CreatureID` = @mailboxEntry;

INSERT INTO `creature_template` (`entry`, `name`, `minlevel`, `maxlevel`, `faction`, `npcflag`, `unit_class`, `unit_flags`, `type`, `AIName`)
VALUES (@mailboxEntry, 'Archipelago Stone Mailbox', 1, 1, 35, 67108864, 1, 33555202, 10, 'NullCreatureAI');

INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`)
VALUES (@mailboxEntry, 0, @mailboxDisplayId, 1, 1);
