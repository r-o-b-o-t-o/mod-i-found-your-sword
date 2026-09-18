DROP TABLE IF EXISTS `location_check`;
CREATE TABLE `location_check` (
  `guid` int unsigned NOT NULL,
  `locationId` int NOT NULL,
  `time` datetime NOT NULL DEFAULT NOW(),
  PRIMARY KEY (`guid`, `locationId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Archipelago locations each character has checked.';
