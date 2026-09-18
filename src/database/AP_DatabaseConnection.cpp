#include "database/AP_DatabaseConnection.h"
#include "MySQLConnection.h"
#include "MySQLPreparedStatement.h"

namespace ModArchipelaWoW::Database
{
    AP_DatabaseConnection::AP_DatabaseConnection(MySQLConnectionInfo& connInfo) :
        MySQLConnection(connInfo)
    {
    }

    void AP_DatabaseConnection::DoPrepareStatements()
    {
        if (!m_reconnecting)
        {
            m_stmts.resize(MAX_AP_DATABASE_STATEMENTS);
        }

        // A module pool only opens synchronous connections, so every statement is CONNECTION_SYNCH.
        PrepareStatement(AP_SEL_CHARACTER, "SELECT `uuid`, `slot`, `itemIndex`, `apLevel`, `apExp`, `goalCompleted` FROM `characters` WHERE `guid` = ?", CONNECTION_SYNCH);
        PrepareStatement(AP_SEL_CHARACTER_BY_SLOT, "SELECT 1 FROM `characters` WHERE `slot` = ?", CONNECTION_SYNCH);
        PrepareStatement(AP_REP_CHARACTER, "REPLACE INTO `characters` (`guid`, `uuid`, `slot`, `itemIndex`, `apLevel`, `apExp`, `goalCompleted`) VALUES (?, ?, ?, ?, ?, ?, ?)", CONNECTION_SYNCH);
        PrepareStatement(AP_DEL_CHARACTER, "DELETE FROM `characters` WHERE `guid` = ?", CONNECTION_SYNCH);
        PrepareStatement(AP_SEL_LOCATION_CHECKS, "SELECT `locationId` FROM `location_check` WHERE `guid` = ?", CONNECTION_SYNCH);
        PrepareStatement(AP_REP_LOCATION_CHECK, "REPLACE INTO `location_check` (`guid`, `locationId`) VALUES (?, ?)", CONNECTION_SYNCH);
        PrepareStatement(AP_DEL_LOCATION_CHECKS, "DELETE FROM `location_check` WHERE `guid` = ?", CONNECTION_SYNCH);
        PrepareStatement(AP_SEL_PLAYER_CREATURE_TEMPLATES, "SELECT `player`, `creatureEntry` FROM `player_creature_template`", CONNECTION_SYNCH);
        PrepareStatement(AP_INS_PLAYER_CREATURE_TEMPLATE, "INSERT INTO `player_creature_template` (`player`, `creatureEntry`) VALUES (?, ?)", CONNECTION_SYNCH);
    }
}
