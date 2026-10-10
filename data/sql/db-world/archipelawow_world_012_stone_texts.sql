-- Gossip texts of the Archipelago Stone's other menus.

SET @mainTextId := 100502;
SET @zoneTextId := 100503;
SET @zoneListTextId := 100504;
SET @dungeonTextId := 100505;
SET @dungeonListTextId := 100506;

DELETE FROM `npc_text` WHERE `ID` IN (@mainTextId, @zoneTextId, @zoneListTextId, @dungeonTextId, @dungeonListTextId);

INSERT INTO `npc_text` (`ID`, `text0_0`)
VALUES
(@mainTextId, 'The stone hums softly in your hand. What would you like to do?'),
(@zoneTextId, 'Which continent would you like to travel to?'),
(@zoneListTextId, 'Where would you like to go?'),
(@dungeonTextId, 'Which era of dungeons would you like to visit?'),
(@dungeonListTextId, 'Which dungeon would you like to travel to?');
