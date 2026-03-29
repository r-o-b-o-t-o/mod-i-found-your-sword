#include "AP_Config.h"
#include "ConfigValueCache.h"
#include "Define.h"

#include <string>

namespace ModArchipelaWoW
{
    Config::Config() :
        ConfigValueCache(ConfigField::NUM_CONFIGS)
    {
    }

    void Config::BuildConfigCache()
    {
        SetConfigValue<bool>(ConfigField::ENABLE, "ArchipelaWoW.Enable", true);
        SetConfigValue<bool>(ConfigField::ANNOUNCE, "ArchipelaWoW.Announce", true);
        SetConfigValue<std::string>(ConfigField::ARCHIPELAGO_SERVER_HOST, "ArchipelaWoW.ArchipelagoServerHost", "archipelago.gg");
        SetConfigValue<uint32>(ConfigField::ARCHIPELAGO_SERVER_PORT, "ArchipelaWoW.ArchipelagoServerPort", static_cast<uint32>(38281));
        SetConfigValue<std::string>(ConfigField::ARCHIPELAGO_PASSWORD, "ArchipelaWoW.ArchipelagoPassword", "");
    }

    bool Config::IsEnabled() const
    {
        return GetConfigValue<bool>(ConfigField::ENABLE);
    }

    bool Config::ShouldAnnounce() const
    {
        return GetConfigValue<bool>(ConfigField::ANNOUNCE);
    }

    std::string Config::GetArchipelagoServerHost() const
    {
        return GetConfigValue<std::string>(ConfigField::ARCHIPELAGO_SERVER_HOST);
    }

    uint32 Config::GetArchipelagoServerPort() const
    {
        return GetConfigValue<uint32>(ConfigField::ARCHIPELAGO_SERVER_PORT);
    }

    std::string Config::GetArchipelagoPassword() const
    {
        return GetConfigValue<std::string>(ConfigField::ARCHIPELAGO_PASSWORD);
    }
}
