-- The client checks the use spell's attributes in its own Spell.dbc before it sends the item use, so
-- the only way to let the stone open its menu while mounted is to point it at a spell the client
-- already allows there. The previous spell (36177) lacked "castable while mounted": the client
-- dismounted a character from a ground mount to use the stone and refused it outright on a taxi.
--
-- 41024 (Dragonmaw Knockdown Choose Loc) is a self-targeted dummy like 36177: instant, no cost, no
-- cooldown, no global cooldown, no visual, no tooltip text and hidden from the combat log, so its name
-- never reaches the player. Nothing in the core or the world database casts or handles it; the one
-- spell that triggers it (41023) is never cast either. It adds "castable while mounted".

SET @apStoneId := 32618;
UPDATE `item_template` SET `spellid_1` = 41024 WHERE `entry` = @apStoneId;
