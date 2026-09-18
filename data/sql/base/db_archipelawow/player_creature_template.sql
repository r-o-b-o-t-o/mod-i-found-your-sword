DROP TABLE IF EXISTS `player_creature_template`;
CREATE TABLE `player_creature_template` (
  `player` varchar(100) NOT NULL,
  `creatureEntry` int unsigned NOT NULL,
  PRIMARY KEY (`player`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='creature_template entry standing in for each Archipelago player as a mail sender.';
