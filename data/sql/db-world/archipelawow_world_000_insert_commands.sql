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

CALL ArchipelaWoW_InsertCommand('connect', 0, 'connect $slotName\nConnect the current character to the configured Archipelago server.\n$slotName: The slot name to bind.');
DROP PROCEDURE ArchipelaWoW_InsertCommand;
