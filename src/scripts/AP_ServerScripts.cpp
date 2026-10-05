#include "ArchipelaWoW.h"
#include "IoContext.h"
#include "scripts/AP_ServerScripts.h"
#include "ServerScript.h"
#include "WorldPacket.h"
#include "WorldSession.h"

namespace ModArchipelaWoW::Scripts
{
    class AP_ServerScript : public ServerScript
    {
    public:
        AP_ServerScript() :
            ServerScript("ArchipelaWoW_ServerScript", {
                SERVERHOOK_ON_NETWORK_START,
#ifdef MOD_ARCHIPELAWOW_TRANSMOG
                SERVERHOOK_CAN_PACKET_SEND,
                SERVERHOOK_CAN_PACKET_RECEIVE,
#endif
            })
        {
        }

        void OnNetworkStart(Acore::Asio::IoContext& ioContext) override
        {
            sArchipelaWoW->OnNetworkStart(ioContext);
        }

#ifdef MOD_ARCHIPELAWOW_TRANSMOG
        bool CanPacketSend(WorldSession* session, const WorldPacket& packet) override
        {
            return sArchipelaWoW->CanPacketSend(session, packet);
        }

        bool CanPacketReceive(WorldSession* session, const WorldPacket& packet) override
        {
            return sArchipelaWoW->CanPacketReceive(session, packet);
        }
#endif
    };

    void AddServerScripts()
    {
        new AP_ServerScript();
    }
}
