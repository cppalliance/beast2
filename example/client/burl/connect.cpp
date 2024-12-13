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
#include <boost/asio/ssl/stream.hpp>
#include <boost/http_io.hpp>
#include <boost/http_proto.hpp>
#include <boost/url.hpp>

namespace core     = boost::core;
namespace grammar  = boost::urls::grammar;
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
        std::uint8_t greeting_resp[2];
        co_await asio::async_read(stream, asio::buffer(greeting_resp));

        if(greeting_resp[1] != 0x00)
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

    std::uint16_t port = std::stoi(effective_port(url));
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
    const po::variables_map& vm,
    http_proto::context& http_proto_ctx,
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

    if(vm.count("user-agent"))
    {
        request.set(field::user_agent, vm.at("user-agent").as<std::string>());
    }
    else
    {
        request.set(field::user_agent, "Boost.Http.Io");
    }

    if(proxy.has_userinfo())
    {
        auto credentials = proxy.encoded_userinfo().decode();
        auto basic_auth  = std::string{ "Basic " };
        base64_encode(basic_auth, credentials);
        request.set(field::proxy_authorization, basic_auth);
    }

    auto serializer = http_proto::serializer{ http_proto_ctx };
    auto parser     = http_proto::response_parser{ http_proto_ctx };

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

    co_await ssl_stream.async_handshake(ssl::stream_base::client);
    co_return ssl_stream;
}
} // namespace

asio::awaitable<void>
connect(
    const po::variables_map& vm,
    ssl::context& ssl_ctx,
    http_proto::context& http_proto_ctx,
    any_stream& stream,
    urls::url url)
{
    auto org_host = url.host();
    auto executor = co_await asio::this_coro::executor;

    if(vm.count("unix-socket") || vm.count("abstract-unix-socket"))
    {
        auto socket = asio::local::stream_protocol::socket{ executor };
        auto path   = [&]() -> std::string
        {
            if(vm.count("abstract-unix-socket"))
                return '\0' + vm.at("abstract-unix-socket").as<std::string>();

            return vm.at("unix-socket").as<std::string>();
        }();
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

    if(vm.count("connect-to"))
    {
        static constexpr auto parser = grammar::tuple_rule(
            urls::authority_rule,
            grammar::squelch(grammar::optional_rule(grammar::delim_rule(':'))),
            urls::authority_rule);

        for(core::string_view sv :
            vm.at("connect-to").as<std::vector<std::string>>())
        {
            auto rs = grammar::parse(sv, parser);
            if(rs.has_error())
                throw std::runtime_error{ "bad --connect-to option" };
            auto [left, right] = rs.value();

            auto left_host = left.encoded_host();
            auto left_port = left.port();

            if(url.encoded_host() != left_host && !left_host.empty())
                continue;

            if(effective_port(url) != left_port && !left_port.empty())
                continue;

            if(!right.encoded_host().empty())
                url.set_encoded_host(right.encoded_host());

            if(!right.port().empty())
                url.set_port(right.port());

            break;
        }
    }

    if(vm.count("resolve"))
    {
        static constexpr auto parser = grammar::tuple_rule(
            grammar::token_rule(grammar::all_chars - grammar::lut_chars(':')),
            grammar::squelch(grammar::delim_rule(':')),
            grammar::token_rule(grammar::digit_chars),
            grammar::squelch(grammar::delim_rule(':')),
            grammar::squelch(grammar::optional_rule(grammar::delim_rule('['))),
            grammar::variant_rule(
                urls::ipv4_address_rule,
                urls::ipv6_address_rule),
            grammar::squelch(grammar::optional_rule(grammar::delim_rule(']'))));

        for(core::string_view sv :
            vm.at("resolve").as<std::vector<std::string>>())
        {
            auto rs = grammar::parse(sv, parser);
            if(rs.has_error())
                throw std::runtime_error{ "bad --resolve option" };
            auto [host, port, ip_address] = rs.value();

            if(url.encoded_host() != host && host != "*")
                continue;

            if(effective_port(url) != port)
                continue;

            if(ip_address.index() == 0)
                url.set_host_ipv4(get<0>(ip_address));
            else
                url.set_host_ipv6(get<1>(ip_address));

            break;
        }
    }

    if(vm.count("proxy"))
    {
        auto proxy_url = urls::parse_uri(vm.at("proxy").as<std::string>());

        if(proxy_url.has_error())
            throw system_error{ proxy_url.error(), "Failed to parse proxy" };

        if(proxy_url->scheme() == "http")
        {
            co_await connect_http_proxy(
                vm, http_proto_ctx, socket, url, proxy_url.value());
        }
        else if(proxy_url->scheme() == "socks5")
        {
            co_await connect_socks5_proxy(socket, url, proxy_url.value());
        }
        else
        {
            throw std::runtime_error{
                "only HTTP and SOCKS5 proxies are supported"
            };
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
                if(vm.count("ipv4") && next.address().is_v6())
                    return false;

                if(vm.count("ipv6") && next.address().is_v4())
                    return false;

                return true;
            });
    }

    if(vm.count("tcp-nodelay"))
        socket.set_option(asio::ip::tcp::no_delay{ true });

    if(vm.count("no-keepalive"))
        socket.set_option(asio::ip::tcp::socket::keep_alive{ false });

    if(url.scheme_id() == urls::scheme::https)
    {
        stream = co_await perform_tls_handshake(
            ssl_ctx, std::move(socket), org_host);
        co_return;
    }
    stream = std::move(socket);
}
