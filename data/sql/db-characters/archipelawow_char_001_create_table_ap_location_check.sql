CREATE TABLE IF NOT EXISTS `ap_location_check` (
    `guid` int unsigned NOT NULL,
    `locationId` int NOT NULL,
    `time` datetime NOT NULL DEFAULT NOW(),
    PRIMARY KEY (`guid`, `locationId`)
);
