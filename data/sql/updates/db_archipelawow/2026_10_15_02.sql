ALTER TABLE `preferences`
  ADD COLUMN `pendingTraps` varchar(255) NOT NULL DEFAULT '' AFTER `casterWeapons`;
