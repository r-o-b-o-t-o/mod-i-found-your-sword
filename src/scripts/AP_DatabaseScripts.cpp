#include "ArchipelaWoW.h"
#include "database/AP_Database.h"
#include "DatabaseScript.h"
#include "scripts/AP_DatabaseScripts.h"

#include <map>
#include <string>

namespace ModArchipelaWoW::Scripts
{
    class AP_DatabaseScript : public DatabaseScript
    {
    public:
        AP_DatabaseScript() :
            DatabaseScript("ArchipelaWoW_DatabaseScript", {
                DATABASEHOOK_ON_MODULE_DATABASES_LOADING,
                DATABASEHOOK_ON_MODULE_DATABASES_KEEPALIVE,
                DATABASEHOOK_ON_MODULE_DATABASES_CLOSING,
                DATABASEHOOK_ON_DATABASE_GET_DB_REVISION,
            })
        {
        }

        // Through the module rather than the pool, for the config it needs. Opened whether or not
        // ArchipelaWoW.Enable is set: the module can be switched on with a config reload.
        bool OnModuleDatabasesLoading() override
        {
            return sArchipelaWoW->OnModuleDatabasesLoading();
        }

        void OnModuleDatabasesKeepAlive() override
        {
            Database::ArchipelaWoWDatabase.KeepAlive();
        }

        void OnModuleDatabasesClosing() override
        {
            Database::ArchipelaWoWDatabase.Close();
        }

        void OnDatabaseGetDBRevision(std::map<std::string, std::string>& revisions) override
        {
            revisions["ArchipelaWoW"] = Database::ArchipelaWoWDatabase.GetRevision();
        }
    };

    void AddDatabaseScripts()
    {
        new AP_DatabaseScript();
    }
}
