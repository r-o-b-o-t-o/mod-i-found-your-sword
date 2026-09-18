DROP TABLE IF EXISTS `updates_include`;
CREATE TABLE `updates_include` (
  `path` varchar(200) NOT NULL COMMENT 'directory to include. $ means relative to the module directory.',
  `state` enum('RELEASED','ARCHIVED','CUSTOM','PENDING') NOT NULL DEFAULT 'RELEASED' COMMENT 'defines if the directory contains released or archived updates.',
  PRIMARY KEY (`path`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='List of directories where we want to include sql updates.';

DELETE FROM `updates_include` WHERE `path` = '$/data/sql/updates/db_archipelawow';
INSERT INTO `updates_include` (`path`, `state`) VALUES
('$/data/sql/updates/db_archipelawow', 'RELEASED');
