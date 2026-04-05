#include "network/AP_WebSocketService.h"
#include "network/AP_WebSocketClient.h"

#include <boost/asio/any_io_executor.hpp>
#include <memory>
#include <utility>

namespace ModArchipelaWoW::Network
{
    WebSocketService::WebSocketService(boost::asio::any_io_executor executor) :
        executor(std::move(executor))
    {
    }

    std::shared_ptr<WebSocketClient> WebSocketService::CreateClient(bool useTls)
    {
        return WebSocketClient::Create(executor, useTls);
    }
}
