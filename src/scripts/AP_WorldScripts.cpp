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
    };

    void AddWorldScripts()
    {
        new AP_WorldScript();
    }
}
