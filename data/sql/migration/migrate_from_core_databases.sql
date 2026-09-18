-- One-time migration for realms that ran the module before it had a database of its own, when
-- its tables lived in the core's characters and world databases.
--
-- Start worldserver once with the new module version so the module database exists, stop it,
-- adjust the three schema names below to your realm, run this against the MySQL server, then
-- start worldserver again.

INSERT INTO `acore_archipelawow`.`characters`
  SELECT * FROM `acore_characters`.`ap_character`;
INSERT INTO `acore_archipelawow`.`location_check`
  SELECT * FROM `acore_characters`.`ap_location_check`;
INSERT INTO `acore_archipelawow`.`player_creature_template`
  SELECT * FROM `acore_world`.`ap_player_creature_template`;

-- The old tables are no longer read. Drop them once the copy above is verified:
-- DROP TABLE `acore_characters`.`ap_character`, `acore_characters`.`ap_location_check`;
-- DROP TABLE `acore_world`.`ap_player_creature_template`;
