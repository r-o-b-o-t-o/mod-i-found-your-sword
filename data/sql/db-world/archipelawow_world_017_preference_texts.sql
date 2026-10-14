-- Gossip texts of the Archipelago Stone's Preferences menus.

SET @preferencesTextId := 100508;
SET @weaponSlotsTextId := 100509;
SET @weaponListTextId := 100510;

DELETE FROM `npc_text` WHERE `ID` IN (@preferencesTextId, @weaponSlotsTextId, @weaponListTextId);

INSERT INTO `npc_text` (`ID`, `text0_0`)
VALUES
(@preferencesTextId, 'How should the Archipelago equip you?'),
(@weaponSlotsTextId, 'Your heirloom weapons arrive in this order. A slot can be changed until its weapon is received.'),
(@weaponListTextId, 'Which weapon would you like in this slot?');
