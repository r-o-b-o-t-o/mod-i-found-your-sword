#include "AP_Config.h"
#include "BuiltInConfig.h"
#include "database/AP_Database.h"
#include "database/AP_DatabaseConnection.h"
#include "DBUpdater.h"
#include "Define.h"
#include "Log.h"
#include "ModuleDatabasePool.h"
#include "MySQLConnection.h"
#include "PreparedStatement.h"
#include "QueryResult.h"
#include "Transaction.h"

#include <memory>
#include <string>
#include <utility>

// ER_BAD_DB_ERROR; the MySQL headers are private to the core's database library.
constexpr uint32 MYSQL_ERROR_BAD_DB = 1049;

namespace ModArchipelaWoW::Database
{
    AP_DatabasePool ArchipelaWoWDatabase;

    bool AP_DatabasePool::Load(const Config& config)
    {
        SetConnectionInfo(config.GetDatabaseInfo(), config.GetDatabaseSynchThreads());

        uint32 error = Open();
        if (error == MYSQL_ERROR_BAD_DB && config.ShouldUpdateDatabase() && config.ShouldCreateDatabase())
        {
            if (ModuleDBUpdater::Create(*this))
            {
                error = Open();
            }
        }

        if (error)
        {
            LOG_ERROR("module.archipelawow", "Could not open the ArchipelaWoW database, check ArchipelaWoW.DatabaseInfo and the errors above.");
            return false;
        }

        if (config.ShouldUpdateDatabase())
        {
            DBUpdaterInfo const updaterInfo = {
                "ArchipelaWoW",
                GetSourceDirectory(),
                GetSourceDirectory() + "/data/sql/base/db_archipelawow/",
                "archipelawow",
            };

            if (!ModuleDBUpdater::Populate(*this, updaterInfo) || !ModuleDBUpdater::Update(*this, updaterInfo))
            {
                LOG_ERROR("module.archipelawow", "Could not populate or update the ArchipelaWoW database, see the errors above.");
                return false;
            }
        }

        if (!PrepareStatements())
        {
            LOG_ERROR("module.archipelawow", "Could not prepare the ArchipelaWoW database statements, see the errors above.");
            return false;
        }

        return true;
    }

    std::string AP_DatabasePool::GetRevision()
    {
        QueryResult result = Query("SELECT `name` FROM `updates` ORDER BY `name` DESC LIMIT 1");
        return result ? (*result)[0].Get<std::string>() : "No updates found!";
    }

    AP_DatabasePreparedStatement* AP_DatabasePool::GetPreparedStatement(AP_DatabaseStatements index)
    {
        return new AP_DatabasePreparedStatement(index, GetPreparedStatementParamCount(index));
    }

    AP_DatabaseTransaction AP_DatabasePool::BeginTransaction()
    {
        return std::make_shared<Transaction<AP_DatabaseConnection>>();
    }

    void AP_DatabasePool::CommitTransaction(AP_DatabaseTransaction transaction)
    {
        DirectCommitTransaction(std::move(transaction));
    }

    MySQLConnection* AP_DatabasePool::CreateConnection(MySQLConnectionInfo& connInfo)
    {
        return new AP_DatabaseConnection(connInfo);
    }

    std::string AP_DatabasePool::GetSourceDirectory()
    {
        // The clone directory name is what the core's module SQL scan and the module's own cmake
        // file already go by, and SourceDirectory in worldserver.conf still relocates the whole tree.
        return BuiltInConfig::GetSourceDirectory() + "/modules/mod-i-found-your-sword";
    }
}
