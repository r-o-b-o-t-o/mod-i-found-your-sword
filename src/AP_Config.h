#ifndef _MOD_ARCHIPELAWOW_CONFIG_H_
#define _MOD_ARCHIPELAWOW_CONFIG_H_

#include "ConfigValueCache.h"
#include "Define.h"

#include <string>

namespace ModArchipelaWoW
{
    enum class ConfigField
    {
        ENABLE,
        ANNOUNCE,
        ARCHIPELAGO_SERVER_HOST,
        ARCHIPELAGO_SERVER_PORT,
        ARCHIPELAGO_PASSWORD,

        NUM_CONFIGS,
    };

    class Config : public ConfigValueCache<ConfigField>
    {
    public:
        Config();
        void BuildConfigCache() override;

        bool IsEnabled() const;
        bool ShouldAnnounce() const;
        std::string GetArchipelagoServerHost() const;
        uint32 GetArchipelagoServerPort() const;
        std::string GetArchipelagoPassword() const;
    };
}

#endif
