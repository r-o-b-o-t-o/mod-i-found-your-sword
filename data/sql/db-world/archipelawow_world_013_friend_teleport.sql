-- The Archipelago Stone's Teleport to Friend menu.
--
-- The serverside-only spell is never cast: it carries the teleport's cooldown, which the core saves
-- with the character and only restores for a spell it knows. Passive and hidden like the movement
-- speed spell, so the client never hears of it. Its recovery time matches the cooldown so that
-- entering an arena, which clears the cooldowns under 10 minutes, leaves it alone.

SET @friendTeleportSpellId := 100501;
SET @friendTeleportTextId := 100507;

DELETE FROM `spell_dbc` WHERE `ID` = @friendTeleportSpellId;
INSERT INTO `spell_dbc` (`ID`, `Attributes`, `CastingTimeIndex`, `DurationIndex`, `RangeIndex`, `RecoveryTime`, `EquippedItemClass`, `SchoolMask`, `Name_Lang_enUS`)
VALUES (@friendTeleportSpellId, 448, 1, 21, 1, 1800000, -1, 1, 'Archipelago Teleport to Friend - serverside spell');

DELETE FROM `npc_text` WHERE `ID` = @friendTeleportTextId;
INSERT INTO `npc_text` (`ID`, `text0_0`)
VALUES (@friendTeleportTextId, 'Which friend would you like to join?');
