#ifndef _MOD_ARCHIPELAWOW_CONFIG_H_
#define _MOD_ARCHIPELAWOW_CONFIG_H_

#include "ConfigValueCache.h"
#include "Define.h"

#include <string>
#include <unordered_set>

namespace ModArchipelaWoW
{
    enum class ConfigField
    {
        ENABLE,
        ANNOUNCE,
        ARCHIPELAGO_SERVER_HOST,
        ARCHIPELAGO_SERVER_PORT,
        ARCHIPELAGO_PASSWORD,
        CHAT_RELAY_SAY,
        CHAT_RELAY_CHANNELS,
        CHAT_COMMAND_PREFIX,

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

        bool IsSayRelayed() const;
        const std::string& GetChatCommandPrefix() const;
        bool IsChatChannelRelayed(const std::string& channelName) const;

    private:
        std::unordered_set<std::string> relayedChannels;
        std::string chatCommandPrefix;

        void BuildChatCache();
    };
}

#endif
