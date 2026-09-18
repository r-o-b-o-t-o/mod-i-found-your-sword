#ifndef _MOD_ARCHIPELAWOW_AP_DATABASE_H_
#define _MOD_ARCHIPELAWOW_AP_DATABASE_H_

#include "AP_Config.h"
#include "database/AP_DatabaseConnection.h"
#include "Define.h"
#include "ModuleDatabasePool.h"
#include "MySQLConnection.h"
#include "PreparedStatement.h"
#include "Transaction.h"

#include <memory>
#include <string>

namespace ModArchipelaWoW::Database
{
    using AP_DatabasePreparedStatement = PreparedStatement<AP_DatabaseConnection>;
    using AP_DatabaseTransaction = std::shared_ptr<Transaction<AP_DatabaseConnection>>;

    class AP_DatabasePool : public ModuleDatabasePool
    {
    public:
        /// Opens the database -- creating, populating and updating it when configured to -- and
        /// prepares the statements. Returns false to abort worldserver startup.
        bool Load(const Config& config);
        /// Name of the last applied update, the way `.server info` reports the core databases.
        std::string GetRevision();

        AP_DatabasePreparedStatement* GetPreparedStatement(AP_DatabaseStatements index);
        AP_DatabaseTransaction BeginTransaction();
        void CommitTransaction(AP_DatabaseTransaction transaction);

    protected:
        MySQLConnection* CreateConnection(MySQLConnectionInfo& connInfo) override;

    private:
        static std::string GetSourceDirectory();
    };

    extern AP_DatabasePool ArchipelaWoWDatabase;
}

#endif
