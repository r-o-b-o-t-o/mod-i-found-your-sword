#include "ArchipelaWoW.h"
#include "DBCEnums.h"
#include "Define.h"
#include "GlobalScript.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "scripts/AP_GlobalScripts.h"
#include "Unit.h"

namespace ModArchipelaWoW::Scripts
{
    class AP_GlobalScript : public GlobalScript
    {
    public:
        AP_GlobalScript() :
            GlobalScript("ArchipelaWoW_GlobalScript", {
                GLOBALHOOK_ON_AFTER_UPDATE_ENCOUNTER_STATE,
            })
        {
        }

        void OnAfterUpdateEncounterState(Map* map, EncounterCreditType type, uint32 creditEntry, Unit* /*source*/, Difficulty /*difficulty_fixed*/,
            const DungeonEncounterList* encounters, uint32 /*dungeonCompleted*/, bool /*updated*/) override
        {
            sArchipelaWoW->OnAfterUpdateEncounterState(map, type, creditEntry, encounters);
        }
    };

    void AddGlobalScripts()
    {
        new AP_GlobalScript();
    }
}
