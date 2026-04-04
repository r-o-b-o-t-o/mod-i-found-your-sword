#ifndef _MOD_ARCHIPELAWOW_NETWORK_WEB_SOCKET_CLIENT_H_
#define _MOD_ARCHIPELAWOW_NETWORK_WEB_SOCKET_CLIENT_H_

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <functional>
#include <memory>
#include <queue>
#include <string>

namespace ModArchipelaWoW::Network
{
    /// A generic asynchronous WebSocket client built on boost::beast.
    /// All callbacks are invoked on the executor's thread.
    /// Public methods are safe to call from any thread (internally dispatched).
    /// Instances must be managed via std::shared_ptr (use Create()).
    class WebSocketClient : public std::enable_shared_from_this<WebSocketClient>
    {
    public:
        enum class State
        {
            Disconnected,
            Connecting,
            Connected,
            Closing
        };

        using OpenHandler = std::function<void()>;
        using CloseHandler = std::function<void()>;
        using MessageHandler = std::function<void(std::string)>;
        using ErrorHandler = std::function<void(boost::beast::error_code)>;

        static std::shared_ptr<WebSocketClient> Create(boost::asio::any_io_executor executor);
        ~WebSocketClient();

        WebSocketClient(const WebSocketClient&) = delete;
        WebSocketClient& operator=(const WebSocketClient&) = delete;

        /// Asynchronously connect to a WebSocket server.
        /// \param host  Hostname or IP address.
        /// \param port  Port number as string.
        /// \param path  Resource path (default "/").
        void Connect(std::string host, std::string port, std::string path = "/");

        /// Queue a text message for sending. Silently ignored if not connected.
        void Send(std::string message);

        /// Initiate a graceful WebSocket close handshake.
        void Close();

        State GetState() const;

        void SetOpenHandler(OpenHandler handler);
        void SetCloseHandler(CloseHandler handler);
        void SetMessageHandler(MessageHandler handler);
        void SetErrorHandler(ErrorHandler handler);

    private:
        explicit WebSocketClient(boost::asio::any_io_executor executor);

        void OnResolve(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type results);
        void OnConnect(boost::beast::error_code ec, boost::asio::ip::tcp::resolver::results_type::endpoint_type endpoint);
        void OnHandshake(boost::beast::error_code ec);
        void OnRead(boost::beast::error_code ec, std::size_t bytesTransferred);
        void OnWrite(boost::beast::error_code ec, std::size_t bytesTransferred);
        void OnClose(boost::beast::error_code ec);

        void DoRead();
        void DoWrite();
        void Fail(boost::beast::error_code ec);

        boost::asio::ip::tcp::resolver resolver;
        boost::beast::websocket::stream<boost::beast::tcp_stream> ws;
        boost::beast::flat_buffer readBuffer;
        std::queue<std::string> writeQueue;
        bool writing;

        std::string host;
        std::string port;
        std::string path;

        State state;

        OpenHandler onOpen;
        CloseHandler onClose;
        MessageHandler onMessage;
        ErrorHandler onError;
    };
}

#endif
