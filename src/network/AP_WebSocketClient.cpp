#include "network/AP_WebSocketClient.h"

#include <boost/asio/any_io_executor.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/ip/basic_resolver_results.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/registered_buffer.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl/stream_base.hpp>
#include <boost/asio/ssl/verify_mode.hpp>
#include <boost/beast/core/bind_handler.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/role.hpp>
#include <boost/beast/core/stream_traits.hpp>
#include <boost/beast/websocket/error.hpp>
#include <boost/beast/websocket/option.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/stream_base.hpp>
#include <chrono>
#include <memory>
#include <openssl/err.h>
#include <openssl/tls1.h>
#include <openssl/x509.h>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <variant>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincrypt.h>
#ifdef _MSC_VER
#pragma comment(lib, "crypt32.lib")
#endif
#endif

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = net::ssl;
using tcp = net::ip::tcp;

namespace ModArchipelaWoW::Network
{
    static constexpr auto kHandshakeTimeout = std::chrono::seconds(30);

    // Matches websocketpp's default; beast's 16 MB default is too small for
    // the data packages of large Archipelago multiworlds.
    static constexpr std::size_t kMaxMessageSize = 32 * 1024 * 1024;

    /// Populate the SSL context with the system's trusted root certificates.
    /// On Windows, OpenSSL's default verify paths are usually empty, so the
    /// certificates are imported from the Windows "ROOT" store instead.
    static void LoadRootCertificates(ssl::context& ctx)
    {
#ifdef _WIN32
        HCERTSTORE store = CertOpenSystemStoreA(0, "ROOT");
        if (!store)
        {
            ctx.set_default_verify_paths();
            return;
        }

        X509_STORE* sslStore = X509_STORE_new();
        PCCERT_CONTEXT cert = nullptr;
        while ((cert = CertEnumCertificatesInStore(store, cert)))
        {
            const unsigned char* encoded = cert->pbCertEncoded;
            if (X509* x509 = d2i_X509(nullptr, &encoded, static_cast<long>(cert->cbCertEncoded)))
            {
                X509_STORE_add_cert(sslStore, x509);
                X509_free(x509);
            }
        }
        CertCloseStore(store, 0);

        // The context takes ownership of the store.
        SSL_CTX_set_cert_store(ctx.native_handle(), sslStore);
#else
        ctx.set_default_verify_paths();
#endif
    }

    std::shared_ptr<WebSocketClient> WebSocketClient::Create(net::any_io_executor executor, bool tls)
    {
        return std::shared_ptr<WebSocketClient>(new WebSocketClient(std::move(executor), tls));
    }

    WebSocketClient::WsStream WebSocketClient::MakeStream(
        net::any_io_executor& executor,
        std::optional<ssl::context>& sslCtx)
    {
        if (sslCtx)
        {
            return TlsStream(executor, *sslCtx);
        }
        return PlainStream(executor);
    }

    WebSocketClient::WebSocketClient(net::any_io_executor executor, bool tls)
        : resolver(executor)
        , sslCtx(tls
            ? std::optional<ssl::context>(ssl::context(ssl::context::tlsv12_client))
            : std::nullopt)
        , ws(MakeStream(executor, sslCtx))
        , writing(false)
        , tls(tls)
        , stopped(false)
        , state(State::Disconnected)
    {
        if (sslCtx)
        {
            sslCtx->set_verify_mode(ssl::verify_peer);
            LoadRootCertificates(*sslCtx);
        }
    }

    WebSocketClient::~WebSocketClient()
    {
        beast::error_code ec;
        VisitStream([&](auto& s) { beast::get_lowest_layer(s).socket().close(ec); });
    }

    void WebSocketClient::Connect(std::string host, std::string port, std::string path)
    {
        auto executor = VisitStream([](auto& s) -> net::any_io_executor { return s.get_executor(); });

        net::dispatch(executor,
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
        auto executor = VisitStream([](auto& s) -> net::any_io_executor { return s.get_executor(); });

        net::dispatch(executor,
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
        auto executor = VisitStream([](auto& s) -> net::any_io_executor { return s.get_executor(); });

        net::dispatch(executor,
            [self = shared_from_this()]()
            {
                if (self->state != State::Connected)
                {
                    return;
                }

                self->state = State::Closing;
                self->VisitStream([&](auto& s)
                {
                    s.async_close(websocket::close_code::normal,
                        beast::bind_front_handler(&WebSocketClient::OnClose, self));
                });
            });
    }

    void WebSocketClient::Stop()
    {
        auto executor = VisitStream([](auto& s) -> net::any_io_executor { return s.get_executor(); });

        net::dispatch(executor,
            [self = shared_from_this()]()
            {
                if (self->stopped.exchange(true, std::memory_order_acq_rel))
                {
                    return;
                }

                self->resolver.cancel();

                beast::error_code ec;
                self->VisitStream([&](auto& s) { beast::get_lowest_layer(s).socket().close(ec); });
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
        if (stopped.load(std::memory_order_acquire))
        {
            return;
        }

        if (ec)
        {
            return Fail(ec);
        }

        VisitStream([&](auto& s)
        {
            beast::get_lowest_layer(s).expires_after(kHandshakeTimeout);
            beast::get_lowest_layer(s).async_connect(results,
                beast::bind_front_handler(&WebSocketClient::OnConnect, shared_from_this()));
        });
    }

    void WebSocketClient::OnConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type)
    {
        if (stopped.load(std::memory_order_acquire))
        {
            return;
        }

        if (ec)
        {
            return Fail(ec);
        }

        if (tls)
        {
            // Keep the deadline active to cover the TLS handshake phase.
            VisitStream([&](auto& s)
            {
                beast::get_lowest_layer(s).expires_after(kHandshakeTimeout);
            });

            auto& tlsStream = std::get<TlsStream>(ws);

            // Set SNI hostname for virtual-hosted TLS servers.
            if (!SSL_set_tlsext_host_name(tlsStream.next_layer().native_handle(), host.c_str()))
            {
                beast::error_code sniEc{
                    static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
                return Fail(sniEc);
            }

            // Enable hostname verification to prevent MITM attacks.
            tlsStream.next_layer().set_verify_callback(ssl::host_name_verification(host));

            tlsStream.next_layer().async_handshake(ssl::stream_base::client,
                beast::bind_front_handler(&WebSocketClient::OnSslHandshake, shared_from_this()));
        }
        else
        {
            VisitStream([&](auto& s)
            {
                beast::get_lowest_layer(s).expires_never();
            });

            auto& plainStream = std::get<PlainStream>(ws);
            plainStream.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

            websocket::permessage_deflate deflate;
            deflate.client_enable = true;
            plainStream.set_option(deflate);

            plainStream.async_handshake(host + ":" + port, path,
                beast::bind_front_handler(&WebSocketClient::OnHandshake, shared_from_this()));
        }
    }

    void WebSocketClient::OnSslHandshake(beast::error_code ec)
    {
        if (stopped.load(std::memory_order_acquire))
        {
            return;
        }

        if (ec)
        {
            return Fail(ec);
        }

        // TLS handshake complete; disable the deadline before the WebSocket handshake.
        VisitStream([&](auto& s)
        {
            beast::get_lowest_layer(s).expires_never();
        });

        auto& tlsStream = std::get<TlsStream>(ws);
        tlsStream.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

        websocket::permessage_deflate deflate;
        deflate.client_enable = true;
        tlsStream.set_option(deflate);

        tlsStream.async_handshake(host + ":" + port, path,
            beast::bind_front_handler(&WebSocketClient::OnHandshake, shared_from_this()));
    }

    void WebSocketClient::OnHandshake(beast::error_code ec)
    {
        if (stopped.load(std::memory_order_acquire))
        {
            return;
        }

        if (ec)
        {
            return Fail(ec);
        }

        VisitStream([&](auto& s)
        {
            s.text(true);
            s.read_message_max(kMaxMessageSize);
        });

        state = State::Connected;
        if (onOpen)
        {
            onOpen();
        }

        DoRead();
    }

    void WebSocketClient::OnRead(beast::error_code ec, std::size_t)
    {
        if (stopped.load(std::memory_order_acquire))
        {
            return;
        }

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

        if (stopped.load(std::memory_order_acquire))
        {
            return;
        }

        if (ec)
        {
            if (state == State::Disconnected || state == State::Closing)
            {
                return;
            }

            return Fail(ec);
        }

        // Fail() may have cleared the queue while this write was in flight.
        if (!writeQueue.empty())
        {
            writeQueue.pop();
        }

        if (!writeQueue.empty() && state == State::Connected)
        {
            DoWrite();
        }
    }

    void WebSocketClient::OnClose(beast::error_code ec)
    {
        state = State::Disconnected;

        if (stopped.load(std::memory_order_acquire))
        {
            return;
        }

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
        VisitStream([&](auto& s)
        {
            s.async_read(readBuffer,
                [weak = weak_from_this()](beast::error_code ec, std::size_t n)
                {
                    if (auto self = weak.lock())
                    {
                        self->OnRead(ec, n);
                    }
                });
        });
    }

    void WebSocketClient::DoWrite()
    {
        writing = true;
        VisitStream([&](auto& s)
        {
            s.async_write(net::buffer(writeQueue.front()),
                [weak = weak_from_this()](beast::error_code ec, std::size_t n)
                {
                    if (auto self = weak.lock())
                    {
                        self->OnWrite(ec, n);
                    }
                });
        });
    }

    void WebSocketClient::Fail(beast::error_code ec)
    {
        state = State::Disconnected;

        std::queue<std::string> empty;
        writeQueue.swap(empty);

        beast::error_code closeEc;
        VisitStream([&](auto& s) { beast::get_lowest_layer(s).socket().close(closeEc); });

        if (stopped.load(std::memory_order_acquire))
        {
            return;
        }

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
