DROP TABLE IF EXISTS `characters`;
CREATE TABLE `characters` (
  `guid` int unsigned NOT NULL,
  `uuid` char(36) NOT NULL DEFAULT '',
  `slot` varchar(16) NOT NULL DEFAULT '',
  `itemIndex` int NOT NULL DEFAULT -1,
  `apLevel` tinyint unsigned NOT NULL DEFAULT 1,
  `apExp` int unsigned NOT NULL DEFAULT 0,
  `goalCompleted` tinyint unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Characters bound to an Archipelago slot and their level progress.';
