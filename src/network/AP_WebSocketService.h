#ifndef _MOD_ARCHIPELAWOW_NETWORK_WEB_SOCKET_SERVICE_H_
#define _MOD_ARCHIPELAWOW_NETWORK_WEB_SOCKET_SERVICE_H_

#include <boost/asio/any_io_executor.hpp>

namespace ModArchipelaWoW::Network
{
    class WebSocketService
    {
    public:
        WebSocketService(boost::asio::any_io_executor executor);

    private:
        boost::asio::any_io_executor executor;
    };
}

#endif
