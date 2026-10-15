#include "AllCreatureScript.h"
#include "AP_Traps.h"
#include "Creature.h"
#include "CreatureData.h"
#include "Define.h"
#include "scripts/AP_CreatureScripts.h"

namespace ModArchipelaWoW::Scripts
{
    class AP_AllCreatureScript : public AllCreatureScript
    {
    public:
        AP_AllCreatureScript() :
            AllCreatureScript("ArchipelaWoW_AllCreatureScript")
        {
        }

        void OnBeforeCreatureSelectLevel(const CreatureTemplate* /*cinfo*/, Creature* /*creature*/, uint8& level) override
        {
            Traps::OnBeforeCreatureSelectLevel(level);
        }
    };

    void AddCreatureScripts()
    {
        new AP_AllCreatureScript();
    }
}
