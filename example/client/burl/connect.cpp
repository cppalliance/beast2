//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include "connect.hpp"
#include "base64.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/http_io.hpp>
#include <boost/http_proto.hpp>
#include <boost/url.hpp>

namespace core     = boost::core;
namespace http_io  = boost::http_io;
using error_code   = boost::system::error_code;
using system_error = boost::system::system_error;

namespace
{
core::string_view
effective_port(const urls::url_view& url)
{
    if(url.has_port())
        return url.port();

    if(url.scheme() == "https")
        return "443";

    if(url.scheme() == "http")
        return "80";

    if(url.scheme() == "socks5")
        return "1080";

    throw std::runtime_error{ "Unsupported scheme" };
}

asio::awaitable<void>
connect_socks5_proxy(
    asio::ip::tcp::socket& stream,
    const urls::url_view& url,
    const urls::url_view& proxy)
{
    auto executor = co_await asio::this_coro::executor;
    auto resolver = asio::ip::tcp::resolver{ executor };
    auto rresults =
        co_await resolver.async_resolve(proxy.host(), effective_port(proxy));

    // Connect to the proxy server
    co_await asio::async_connect(stream, rresults);

    // Greeting request
    if(proxy.has_userinfo())
    {
        std::uint8_t greeting_req[4] = { 0x05, 0x02, 0x00, 0x02 };
        co_await asio::async_write(stream, asio::buffer(greeting_req));
    }
    else
    {
        std::uint8_t greeting_req[3] = { 0x05, 0x01, 0x00 };
        co_await asio::async_write(stream, asio::buffer(greeting_req));
    }

    // Greeting response
    std::uint8_t greeting_resp[2];
    co_await asio::async_read(stream, asio::buffer(greeting_resp));

    if(greeting_resp[0] != 0x05)
        throw std::runtime_error{ "SOCKS5 invalid version" };

    switch(greeting_resp[1])
    {
    case 0x00: // No Authentication
        break;
    case 0x02: // Username/password
    {
        // Authentication request
        auto auth_req = std::string{ 0x01 };

        auto user = proxy.encoded_user();
        auth_req.push_back(static_cast<std::uint8_t>(user.decoded_size()));
        user.decode({}, urls::string_token::append_to(auth_req));

        auto pass = proxy.encoded_password();
        auth_req.push_back(static_cast<std::uint8_t>(pass.decoded_size()));
        pass.decode({}, urls::string_token::append_to(auth_req));

        co_await asio::async_write(stream, asio::buffer(auth_req));

        // Authentication response
        std::uint8_t auth_resp[2];
        co_await asio::async_read(stream, asio::buffer(auth_resp));

        if(auth_resp[1] != 0x00)
            throw std::runtime_error{ "SOCKS5 authentication failed" };
        break;
    }
    default:
        throw std::runtime_error{
            "SOCKS5 no acceptable authentication method"
        };
    }

    // Connection request
    auto conn_req = std::string{ 0x05, 0x01, 0x00, 0x03 };
    auto host     = url.encoded_host();
    conn_req.push_back(static_cast<std::uint8_t>(host.decoded_size()));
    host.decode({}, urls::string_token::append_to(conn_req));

    auto port = static_cast<std::uint16_t>(std::stoul(effective_port(url)));
    conn_req.push_back(static_cast<std::uint8_t>((port >> 8) & 0xFF));
    conn_req.push_back(static_cast<std::uint8_t>(port & 0xFF));

    co_await asio::async_write(stream, asio::buffer(conn_req));

    // Connection response
    std::uint8_t conn_resp_head[5];
    co_await asio::async_read(stream, asio::buffer(conn_resp_head));

    if(conn_resp_head[1] != 0x00)
        throw std::runtime_error{ "SOCKS5 connection request failed" };

    std::string conn_resp_tail;
    conn_resp_tail.resize(
        [&]()
        {
            // subtract 1 because we have pre-read one byte
            switch(conn_resp_head[3])
            {
            case 0x01:
                return 4 + 2 - 1; // ipv4 + port
            case 0x03:
                return conn_resp_head[4] + 2 - 1; // domain name + port
            case 0x04:
                return 16 + 2 - 1; // ipv6 + port
            default:
                throw std::runtime_error{ "SOCKS5 invalid address type" };
            }
        }());
    co_await asio::async_read(stream, asio::buffer(conn_resp_tail));
}

asio::awaitable<void>
connect_http_proxy(
    const operation_config& oc,
    rts::context& rts_ctx,
    asio::ip::tcp::socket& stream,
    const urls::url_view& url,
    const urls::url_view& proxy)
{
    auto executor = co_await asio::this_coro::executor;
    auto resolver = asio::ip::tcp::resolver{ executor };
    auto rresults =
        co_await resolver.async_resolve(proxy.host(), effective_port(proxy));

    // Connect to the proxy server
    co_await asio::async_connect(stream, rresults);

    using field    = http_proto::field;
    auto request   = http_proto::request{};
    auto host_port = [&]()
    {
        auto rs = url.encoded_host().decode();
        rs.push_back(':');
        rs.append(effective_port(url));
        return rs;
    }();

    request.set_method(http_proto::method::connect);
    request.set_target(host_port);
    request.set(field::host, host_port);
    request.set(field::proxy_connection, "keep-alive");
    request.set(field::user_agent, oc.useragent.value_or("Boost.Http.Io"));

    if(proxy.has_userinfo())
    {
        auto credentials = proxy.encoded_userinfo().decode();
        auto basic_auth  = std::string{ "Basic " };
        base64_encode(basic_auth, credentials);
        request.set(field::proxy_authorization, basic_auth);
    }

    auto serializer = http_proto::serializer{ rts_ctx };
    auto parser     = http_proto::response_parser{ rts_ctx };

    serializer.start(request);
    co_await http_io::async_write(stream, serializer);

    parser.reset();
    parser.start();
    co_await http_io::async_read_header(stream, parser);

    if(parser.get().status() != http_proto::status::ok)
        throw std::runtime_error{ "Proxy server rejected the connection" };
}

template<typename Socket>
asio::awaitable<ssl::stream<Socket>>
perform_tls_handshake(ssl::context& ssl_ctx, Socket socket, std::string host)
{
    auto ssl_stream = ssl::stream<Socket>{ std::move(socket), ssl_ctx };

    if(!SSL_set_tlsext_host_name(ssl_stream.native_handle(), host.c_str()))
        throw system_error{ static_cast<int>(::ERR_get_error()),
                            asio::error::get_ssl_category() };

    ssl_stream.set_verify_callback(ssl::host_name_verification(host));

    co_await ssl_stream.async_handshake(ssl::stream_base::client);
    co_return ssl_stream;
}
} // namespace

asio::awaitable<void>
connect(
    const operation_config& oc,
    ssl::context& ssl_ctx,
    rts::context& rts_ctx,
    any_stream& stream,
    urls::url url)
{
    auto org_host = url.host();
    auto executor = co_await asio::this_coro::executor;

    if(!oc.unix_socket_path.empty())
    {
        auto socket   = asio::local::stream_protocol::socket{ executor };
        auto path     = oc.unix_socket_path.string();
        auto endpoint = asio::local::stream_protocol::endpoint{ path };
        co_await socket.async_connect(endpoint);

        if(url.scheme_id() == urls::scheme::https)
        {
            stream = co_await perform_tls_handshake(
                ssl_ctx, std::move(socket), org_host);
            co_return;
        }
        stream = std::move(socket);
        co_return;
    }

    auto socket = asio::ip::tcp::socket{ executor };

    if(oc.connect_to)
        oc.connect_to(url);

    if(oc.resolve_to)
        oc.resolve_to(url);

    if(!oc.proxy.empty())
    {
        if(oc.proxy.scheme() == "http")
        {
            co_await connect_http_proxy(oc, rts_ctx, socket, url, oc.proxy);
        }
        else if(oc.proxy.scheme() == "socks5")
        {
            co_await connect_socks5_proxy(socket, url, oc.proxy);
        }
        else
        {
            throw std::runtime_error(
                "only HTTP and SOCKS5 proxies are supported");
        }
    }
    else // no proxy
    {
        auto resolver = asio::ip::tcp::resolver{ executor };
        auto rresults =
            co_await resolver.async_resolve(url.host(), effective_port(url));

        co_await asio::async_connect(
            socket,
            rresults,
            [&](const error_code&, const asio::ip::tcp::endpoint& next)
            {
                if(oc.ipv4 && next.address().is_v6())
                    return false;

                if(oc.ipv6 && next.address().is_v4())
                    return false;

                return true;
            });
    }

    if(oc.tcp_nodelay)
        socket.set_option(asio::ip::tcp::no_delay{ true });

    if(oc.nokeepalive)
        socket.set_option(asio::ip::tcp::socket::keep_alive{ false });

    if(url.scheme_id() == urls::scheme::https)
    {
        stream = co_await perform_tls_handshake(
            ssl_ctx, std::move(socket), org_host);
        co_return;
    }
    stream = std::move(socket);
}
