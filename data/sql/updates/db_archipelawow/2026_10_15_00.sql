ALTER TABLE `preferences`
  ADD COLUMN `casterWorldBuffs` tinyint unsigned NOT NULL DEFAULT 0 AFTER `heirloomWeaponsSent`,
  ADD COLUMN `pendingWorldBuffs` tinyint unsigned NOT NULL DEFAULT 0 AFTER `casterWorldBuffs`;
