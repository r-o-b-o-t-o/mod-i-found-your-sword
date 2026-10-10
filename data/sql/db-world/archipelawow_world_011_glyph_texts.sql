-- Gossip texts of the Archipelago Stone's glyph menus.

SET @glyphsTextId := 100500;
SET @glyphSlotsTextId := 100501;

DELETE FROM `npc_text` WHERE `ID` IN (@glyphsTextId, @glyphSlotsTextId);

INSERT INTO `npc_text` (`ID`, `text0_0`)
VALUES
(@glyphsTextId, 'Select a glyph to inscribe.'),
(@glyphSlotsTextId, 'Which slot would you like to inscribe it to?');
