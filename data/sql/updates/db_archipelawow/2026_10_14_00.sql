CREATE TABLE IF NOT EXISTS `preferences` (
  `guid` int unsigned NOT NULL,
  `casterHeirlooms` tinyint unsigned NOT NULL DEFAULT 0,
  `allArmorTypes` tinyint unsigned NOT NULL DEFAULT 0,
  `heirloomWeapon1` int unsigned NOT NULL DEFAULT 0,
  `heirloomWeapon2` int unsigned NOT NULL DEFAULT 0,
  `heirloomWeapon3` int unsigned NOT NULL DEFAULT 0,
  `heirloomWeaponsSent` tinyint unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='What each character chose in the Archipelago Stone''s Preferences menu, and how many of its heirloom weapon slots were sent.';
