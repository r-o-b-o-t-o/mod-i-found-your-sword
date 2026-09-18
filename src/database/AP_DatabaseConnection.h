#ifndef _MOD_ARCHIPELAWOW_AP_DATABASE_CONNECTION_H_
#define _MOD_ARCHIPELAWOW_AP_DATABASE_CONNECTION_H_

#include "Define.h"
#include "MySQLConnection.h"

namespace ModArchipelaWoW::Database
{
    enum AP_DatabaseStatements : uint32
    {
        AP_SEL_CHARACTER,
        AP_SEL_CHARACTER_BY_SLOT,
        AP_REP_CHARACTER,
        AP_DEL_CHARACTER,
        AP_SEL_LOCATION_CHECKS,
        AP_REP_LOCATION_CHECK,
        AP_DEL_LOCATION_CHECKS,
        AP_SEL_PLAYER_CREATURE_TEMPLATES,
        AP_INS_PLAYER_CREATURE_TEMPLATE,

        MAX_AP_DATABASE_STATEMENTS
    };

    class AP_DatabaseConnection : public MySQLConnection
    {
    public:
        AP_DatabaseConnection(MySQLConnectionInfo& connInfo);

        void DoPrepareStatements() override;
    };
}

#endif
