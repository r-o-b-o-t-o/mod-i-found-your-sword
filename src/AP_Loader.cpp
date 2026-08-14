#include "ArchipelaWoW.h"
#include "scripts/AP_CommandScripts.h"
#include "scripts/AP_ItemScripts.h"
#include "scripts/AP_PlayerScripts.h"
#include "scripts/AP_ServerScripts.h"
#include "scripts/AP_WorldScripts.h"

void Addmod_i_found_your_swordScripts()
{
    ModArchipelaWoW::ArchipelaWoW::Instance(); // Initialize instance
    ModArchipelaWoW::Scripts::AddCommandScripts();
    ModArchipelaWoW::Scripts::AddItemScripts();
    ModArchipelaWoW::Scripts::AddPlayerScripts();
    ModArchipelaWoW::Scripts::AddServerScripts();
    ModArchipelaWoW::Scripts::AddWorldScripts();
}
