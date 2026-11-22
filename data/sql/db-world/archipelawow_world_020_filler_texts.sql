-- The amounts the filler items' descriptions gave are player options now, which the item templates are
-- shared across, so the descriptions no longer name them.

UPDATE `item_template` SET `description` = 'Reveals unchecked locations of your world.' WHERE `entry` = 4427;
UPDATE `item_template` SET `description` = 'Grants experience toward your next Archipelago level.' WHERE `entry` = 1335;
UPDATE `item_template` SET `description` = 'Stores the soul of your targeted group member, or your own. Upon death, they can come back to life where they fell.' WHERE `entry` = 2767;
