#include "network/AP_WebSocketService.h"

#include <boost/asio/any_io_executor.hpp>
#include <utility>

namespace ModArchipelaWoW::Network
{
    WebSocketService::WebSocketService(boost::asio::any_io_executor executor) :
        executor(std::move(executor))
    {
    }
}
