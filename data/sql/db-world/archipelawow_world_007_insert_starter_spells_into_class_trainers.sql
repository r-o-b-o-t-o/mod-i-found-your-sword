-- Puts the abilities a character is created knowing on its class trainer's list.
--
-- With the apworld's "Starter abilities" option on, a character's opening kit is randomized like
-- any other spell: the module takes the abilities back when the slot connects and the multiworld
-- hands them out again. That needs somewhere to buy the check, and the game gives these abilities
-- away at character creation rather than selling them, so they appear on no trainer at all.
--
-- They cost nothing and ask for level 1, which is what a starting ability is worth. With the option
-- off -- and for anyone on the realm not playing Archipelago -- the character already knows them,
-- so the trainer greys them out and nothing changes.
--
-- The spell list is the one tools/generate_spells.py extracts for the apworld, minus the death
-- knight: the apworld cannot roll that class, so its trainers are left alone. A hunter's Auto Shot
-- is left alone too -- the extract excludes it, since a hunter with no ranged attack has no class
-- left to play until the multiworld hands it back.
--
-- The list is held in a temporary table so it is written once: the DELETE that makes this file
-- re-runnable and the INSERT that does the work read the same rows.

CREATE TEMPORARY TABLE `ap_starter_spells` (
    `classId` INT UNSIGNED NOT NULL,
    `spellId` INT UNSIGNED NOT NULL,
    PRIMARY KEY (`classId`, `spellId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

INSERT INTO `ap_starter_spells` (`classId`, `spellId`) VALUES
(1, 78),     -- Warrior: Heroic Strike
(1, 2457),   -- Warrior: Battle Stance
(2, 635),    -- Paladin: Holy Light
(2, 21084),  -- Paladin: Seal of Righteousness
(3, 2973),   -- Hunter: Raptor Strike
(4, 1752),   -- Rogue: Sinister Strike
(4, 2098),   -- Rogue: Eviscerate
(5, 585),    -- Priest: Smite
(5, 2050),   -- Priest: Lesser Heal
(7, 331),    -- Shaman: Healing Wave
(7, 403),    -- Shaman: Lightning Bolt
(8, 133),    -- Mage: Fireball
(8, 168),    -- Mage: Frost Armor
(9, 686),    -- Warlock: Shadow Bolt
(9, 687),    -- Warlock: Demon Skin
(11, 5176),  -- Druid: Wrath
(11, 5185);  -- Druid: Healing Touch

DELETE `ts` FROM `trainer_spell` `ts`
JOIN `trainer` `t` ON `t`.`Id` = `ts`.`TrainerId`
JOIN `ap_starter_spells` `s` ON `s`.`classId` = `t`.`Requirement` AND `s`.`spellId` = `ts`.`SpellId`
WHERE `t`.`Type` = 0;

INSERT INTO `trainer_spell` (`TrainerId`, `SpellId`, `MoneyCost`, `ReqSkillLine`, `ReqSkillRank`, `ReqAbility1`, `ReqAbility2`, `ReqAbility3`, `ReqLevel`, `VerifiedBuild`)
SELECT `t`.`Id`, `s`.`spellId`, 0, 0, 0, 0, 0, 0, 1, 0
FROM `trainer` `t`
JOIN `ap_starter_spells` `s` ON `s`.`classId` = `t`.`Requirement`
WHERE `t`.`Type` = 0;

DROP TEMPORARY TABLE `ap_starter_spells`;
