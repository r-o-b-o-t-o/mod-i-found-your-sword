ALTER TABLE `preferences`
  ADD COLUMN `casterWeapons` tinyint unsigned NOT NULL DEFAULT 0 AFTER `pendingWorldBuffs`;
