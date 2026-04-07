#include "ArchipelaWoW.h"
#include "Define.h"
#include "scripts/AP_WorldScripts.h"
#include "WorldScript.h"

namespace ModArchipelaWoW::Scripts
{
    class AP_WorldScript : public WorldScript
    {
    public:
        AP_WorldScript() :
            WorldScript("ArchipelaWoW_WorldScript", {
                WORLDHOOK_ON_BEFORE_CONFIG_LOAD,
                WORLDHOOK_ON_UPDATE,
                WORLDHOOK_ON_SHUTDOWN,
            })
        {
        }

        void OnBeforeConfigLoad(bool reload) override
        {
            sArchipelaWoW->OnBeforeConfigLoad(reload);
        }

        void OnUpdate(uint32 diff) override
        {
            sArchipelaWoW->OnWorldUpdate(diff);
        }

        void OnShutdown() override
        {
            sArchipelaWoW->OnShutdown();
        }
    };

    void AddWorldScripts()
    {
        new AP_WorldScript();
    }
}
