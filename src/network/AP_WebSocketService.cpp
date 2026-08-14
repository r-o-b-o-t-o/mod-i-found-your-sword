#include "network/AP_WebSocketClient.h"
#include "network/AP_WebSocketService.h"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/strand.hpp>
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
        // The worldserver io_context is run by multiple threads (ThreadPool config);
        // give each client its own strand so its handlers never run concurrently.
        return WebSocketClient::Create(boost::asio::make_strand(executor), useTls);
    }
}
