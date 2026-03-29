#include "ArchipelaWoW.h"
#include "Chat.h"
#include "ChatCommand.h"
#include "CommandScript.h"
#include "Common.h"
#include "Player.h"
#include "scripts/AP_CommandScripts.h"

#include <string>

using namespace Acore::ChatCommands;

namespace ModArchipelaWoW::Scripts
{
    class AP_CommandScript : public CommandScript
    {
    public:
        AP_CommandScript() :
            CommandScript("ArchipelaWoW_CommandScript")
        {
        }

        ChatCommandTable GetCommands() const override
        {
            static ChatCommandTable APCommandTable =
            {
                { "connect", HandleAPConnectCommand, SEC_PLAYER, Console::No },
            };

            static ChatCommandTable commandTable =
            {
                { "archipelago",  APCommandTable },
                { "archipelawow", APCommandTable },
                { "ap",           APCommandTable },
            };

            return commandTable;
        };

    private:
        static bool HandleAPConnectCommand(ChatHandler* handler, const char* args)
        {
            Player* player = handler->GetPlayer();
            if (!player)
            {
                return false;
            }

            std::string slot(args);
            if (slot.empty())
            {
                return false;
            }

            return sArchipelaWoW->HandleAPConnectCommand(player, slot);
        }
    };

    void AddCommandScripts()
    {
        new AP_CommandScript();
    }
}
