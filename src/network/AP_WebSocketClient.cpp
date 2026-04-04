#include "network/AP_WebSocketClient.h"

#include <boost/asio/dispatch.hpp>
#include <chrono>
#include <utility>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

namespace ModArchipelaWoW::Network
{
    std::shared_ptr<WebSocketClient> WebSocketClient::Create(net::any_io_executor executor)
    {
        return std::shared_ptr<WebSocketClient>(new WebSocketClient(std::move(executor)));
    }

    WebSocketClient::WebSocketClient(net::any_io_executor executor)
        : resolver(executor)
        , ws(executor)
        , writing(false)
        , state(State::Disconnected)
    {
    }

    WebSocketClient::~WebSocketClient()
    {
        beast::error_code ec;
        beast::get_lowest_layer(ws).socket().close(ec);
    }

    void WebSocketClient::Connect(std::string host, std::string port, std::string path)
    {
        net::dispatch(ws.get_executor(),
            [self = shared_from_this(), h = std::move(host), p = std::move(port), pa = std::move(path)]() mutable
            {
                if (self->state != State::Disconnected)
                {
                    return;
                }

                self->host = std::move(h);
                self->port = std::move(p);
                self->path = std::move(pa);
                self->state = State::Connecting;

                self->resolver.async_resolve(self->host, self->port,
                    beast::bind_front_handler(&WebSocketClient::OnResolve, self));
            });
    }

    void WebSocketClient::Send(std::string message)
    {
        net::dispatch(ws.get_executor(),
            [self = shared_from_this(), msg = std::move(message)]() mutable
            {
                if (self->state != State::Connected)
                {
                    return;
                }

                self->writeQueue.push(std::move(msg));
                if (!self->writing)
                {
                    self->DoWrite();
                }
            });
    }

    void WebSocketClient::Close()
    {
        net::dispatch(ws.get_executor(),
            [self = shared_from_this()]()
            {
                if (self->state != State::Connected)
                {
                    return;
                }

                self->state = State::Closing;
                self->ws.async_close(websocket::close_code::normal,
                    beast::bind_front_handler(&WebSocketClient::OnClose, self));
            });
    }

    WebSocketClient::State WebSocketClient::GetState() const
    {
        return state;
    }

    void WebSocketClient::SetOpenHandler(OpenHandler handler) { onOpen = std::move(handler); }
    void WebSocketClient::SetCloseHandler(CloseHandler handler) { onClose = std::move(handler); }
    void WebSocketClient::SetMessageHandler(MessageHandler handler) { onMessage = std::move(handler); }
    void WebSocketClient::SetErrorHandler(ErrorHandler handler) { onError = std::move(handler); }

    void WebSocketClient::OnResolve(beast::error_code ec, tcp::resolver::results_type results)
    {
        if (ec)
        {
            return Fail(ec);
        }

        beast::get_lowest_layer(ws).expires_after(std::chrono::seconds(30));
        beast::get_lowest_layer(ws).async_connect(results,
            beast::bind_front_handler(&WebSocketClient::OnConnect, shared_from_this()));
    }

    void WebSocketClient::OnConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type)
    {
        if (ec)
        {
            return Fail(ec);
        }

        beast::get_lowest_layer(ws).expires_never();
        ws.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

        ws.async_handshake(host + ":" + port, path,
            beast::bind_front_handler(&WebSocketClient::OnHandshake, shared_from_this()));
    }

    void WebSocketClient::OnHandshake(beast::error_code ec)
    {
        if (ec)
        {
            return Fail(ec);
        }

        state = State::Connected;
        if (onOpen)
        {
            onOpen();
        }

        DoRead();
    }

    void WebSocketClient::OnRead(beast::error_code ec, std::size_t)
    {
        if (ec)
        {
            if (state == State::Disconnected || state == State::Closing)
            {
                return;
            }

            if (ec == websocket::error::closed)
            {
                state = State::Disconnected;
                if (onClose)
                {
                    onClose();
                }
                return;
            }

            return Fail(ec);
        }

        if (onMessage)
        {
            onMessage(beast::buffers_to_string(readBuffer.data()));
        }

        readBuffer.consume(readBuffer.size());
        DoRead();
    }

    void WebSocketClient::OnWrite(beast::error_code ec, std::size_t)
    {
        writing = false;

        if (ec)
        {
            if (state == State::Disconnected || state == State::Closing)
            {
                return;
            }

            return Fail(ec);
        }

        writeQueue.pop();
        if (!writeQueue.empty() && state == State::Connected)
        {
            DoWrite();
        }
    }

    void WebSocketClient::OnClose(beast::error_code ec)
    {
        state = State::Disconnected;

        if (ec && ec != websocket::error::closed)
        {
            if (onError)
            {
                onError(ec);
            }
        }

        if (onClose)
        {
            onClose();
        }
    }

    void WebSocketClient::DoRead()
    {
        ws.async_read(readBuffer,
            beast::bind_front_handler(&WebSocketClient::OnRead, shared_from_this()));
    }

    void WebSocketClient::DoWrite()
    {
        writing = true;
        ws.async_write(net::buffer(writeQueue.front()),
            beast::bind_front_handler(&WebSocketClient::OnWrite, shared_from_this()));
    }

    void WebSocketClient::Fail(beast::error_code ec)
    {
        state = State::Disconnected;

        std::queue<std::string> empty;
        writeQueue.swap(empty);

        beast::error_code closeEc;
        beast::get_lowest_layer(ws).socket().close(closeEc);

        if (onError)
        {
            onError(ec);
        }

        if (onClose)
        {
            onClose();
        }
    }
}
