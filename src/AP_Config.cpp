#include "AP_Config.h"
#include "ConfigValueCache.h"
#include "Define.h"
#include "Log.h"
#include "Tokenize.h"
#include "Util.h"

#include <string>
#include <string_view>

namespace ModArchipelaWoW
{
    static std::string_view Trim(std::string_view value)
    {
        size_t first = value.find_first_not_of(" \t");
        if (first == std::string_view::npos)
        {
            return {};
        }

        return value.substr(first, value.find_last_not_of(" \t") - first + 1);
    }

    // Folded the way the core identifies a channel, over the whole UTF-8 string rather than byte
    // by byte, so an accented channel name matches whatever case a player typed to join it.
    static std::string NormalizeChannelName(std::string_view name)
    {
        std::wstring wide;
        std::string normalized;
        if (!Utf8toWStr(name, wide))
        {
            return std::string(name);
        }

        wstrToLower(wide);

        if (!WStrToUtf8(wide, normalized))
        {
            return std::string(name);
        }

        return normalized;
    }

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
        SetConfigValue<bool>(ConfigField::CHAT_RELAY_SAY, "ArchipelaWoW.Chat.RelaySay", true);
        SetConfigValue<std::string>(ConfigField::CHAT_RELAY_CHANNELS, "ArchipelaWoW.Chat.RelayChannels", "archipelago");
        SetConfigValue<std::string>(ConfigField::CHAT_COMMAND_PREFIX, "ArchipelaWoW.Chat.CommandPrefix", "!!");

        BuildChatCache();
    }

    void Config::BuildChatCache()
    {
        relayedChannels.clear();

        // Held in a local: Acore::Tokenize deletes its rvalue overloads, and the views it hands
        // back point into whatever string is passed in.
        std::string channels = GetConfigValue<std::string>(ConfigField::CHAT_RELAY_CHANNELS);
        for (std::string_view entry : Acore::Tokenize(channels, ',', false))
        {
            std::string_view name = Trim(entry);
            if (!name.empty())
            {
                relayedChannels.insert(NormalizeChannelName(name));
            }
        }

        chatCommandPrefix = GetConfigValue<std::string>(ConfigField::CHAT_COMMAND_PREFIX);

        // ChatHandler::ParseCommands takes both '.' and '!' as core command prefixes, and it runs
        // on every line before any chat hook does, so a prefix it recognizes never reaches this
        // module. Its own guard against a doubled marker is the way through -- "!!" and ".." are
        // handed back to the chat pipeline -- and so is any prefix starting with something else.
        if (!chatCommandPrefix.empty()
            && (chatCommandPrefix[0] == '!' || chatCommandPrefix[0] == '.')
            && (chatCommandPrefix.size() < 2 || chatCommandPrefix[1] != chatCommandPrefix[0]))
        {
            LOG_ERROR("module.archipelawow", "ArchipelaWoW.Chat.CommandPrefix \"{}\" is swallowed by the core command parser "
                "before the module sees it. Double the first character (\"{}{}\") or start it with another one.",
                chatCommandPrefix, chatCommandPrefix[0], chatCommandPrefix);
        }
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

    bool Config::IsSayRelayed() const
    {
        return GetConfigValue<bool>(ConfigField::CHAT_RELAY_SAY);
    }

    const std::string& Config::GetChatCommandPrefix() const
    {
        return chatCommandPrefix;
    }

    bool Config::IsChatChannelRelayed(const std::string& channelName) const
    {
        // Both checked before folding anything: this runs on every line typed in a channel. A realm
        // relaying none has nothing to compare against, and a channel already spelled the way the
        // config has it -- which is what /join archipelago gives -- matches as it comes in. Only a
        // difference in case pays for the conversion.
        if (relayedChannels.empty())
        {
            return false;
        }

        if (relayedChannels.contains(channelName))
        {
            return true;
        }

        return relayedChannels.contains(NormalizeChannelName(channelName));
    }
}
