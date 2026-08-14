#include "ArchipelaWoW.h"
#include "IoContext.h"
#include "scripts/AP_ServerScripts.h"
#include "ServerScript.h"

namespace ModArchipelaWoW::Scripts
{
    class AP_ServerScript : public ServerScript
    {
    public:
        AP_ServerScript() :
            ServerScript("ArchipelaWoW_ServerScript", {
                SERVERHOOK_ON_NETWORK_START,
            })
        {
        }

        void OnNetworkStart(Acore::Asio::IoContext& ioContext) override
        {
            sArchipelaWoW->OnNetworkStart(ioContext);
        }
    };

    void AddServerScripts()
    {
        new AP_ServerScript();
    }
}
