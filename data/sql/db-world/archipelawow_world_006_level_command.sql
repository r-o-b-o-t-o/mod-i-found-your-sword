-- Registers `.ap level` and its `.ap xp` alias under all three command prefixes, the same way
-- archipelawow_world_000_insert_commands.sql registers `.ap connect`. The procedure is recreated
-- here because that file drops it once it is done.

-- Dropped first, not only at the end: the DROP below runs only if every statement before it
-- succeeded, so without this a half-applied run would leave the procedure behind and the
-- retry would fail on CREATE PROCEDURE.
DROP PROCEDURE IF EXISTS ArchipelaWoW_InsertCommand;

DELIMITER //
CREATE PROCEDURE ArchipelaWoW_InsertCommand(cmdName VARCHAR(50), securityLevel TINYINT unsigned, helpText LONGTEXT)
BEGIN
    REPLACE INTO `command` (`name`, `security`, `help`) VALUES (CONCAT('ap ',           cmdName), securityLevel, CONCAT('Syntax: .ap ',           helpText));
    REPLACE INTO `command` (`name`, `security`, `help`) VALUES (CONCAT('archipelago ',  cmdName), securityLevel, CONCAT('Syntax: .archipelago ',  helpText));
    REPLACE INTO `command` (`name`, `security`, `help`) VALUES (CONCAT('archipelawow ', cmdName), securityLevel, CONCAT('Syntax: .archipelawow ', helpText));
END;
//
DELIMITER ;

CALL ArchipelaWoW_InsertCommand('level', 0, 'level\nShow the Archipelago level of the current character, its experience towards the next level, and how much of that level is done.');
CALL ArchipelaWoW_InsertCommand('xp', 0, 'xp\nAlias of .ap level: show the Archipelago level of the current character, its experience towards the next level, and how much of that level is done.');
DROP PROCEDURE ArchipelaWoW_InsertCommand;
