CREATE TABLE IF NOT EXISTS `ap_character` (
    `guid` int unsigned NOT NULL,
    `uuid` char(36) NOT NULL DEFAULT '',
    `slot` varchar(16) NOT NULL DEFAULT '',
    `itemIndex` int NOT NULL DEFAULT -1,
    `apLevel` tinyint unsigned NOT NULL DEFAULT 1,
    `apExp` int unsigned NOT NULL DEFAULT 0,
    `goalCompleted` tinyint unsigned NOT NULL DEFAULT 0,
    PRIMARY KEY (`guid`)
);
