//
// Copyright (c) 2025 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_EXAMPLE_CLIENT_JSONRPC_LIB_CLIENT_HPP
#define BOOST_HTTP_IO_EXAMPLE_CLIENT_JSONRPC_LIB_CLIENT_HPP

#include "basic_client.hpp"

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/url/url.hpp>

namespace jsonrpc {

class client
    : public basic_client<boost::asio::ssl::stream<
        boost::asio::ip::tcp::socket>>
{
    boost::urls::url url_;
    class connect_op;

public:
    template<typename... Args>
    client(
        boost::urls::url url,
        const boost::rts::context& rts_ctx,
        boost::asio::any_io_executor exec,
        boost::asio::ssl::context& ssl_ctx)
        : basic_client(url, rts_ctx, exec, ssl_ctx)
        , url_(std::move(url))
    {
    }

    template<
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code))
            CompletionToken = boost::asio::deferred_t>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::system::error_code))
    async_connect(CompletionToken&& token = {});

    template<
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code))
            CompletionToken = boost::asio::deferred_t>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::system::error_code))
    async_shutdown(CompletionToken&& token = {})
    {
        return next_layer().
            async_shutdown(std::move(token));
    }
};

class client::connect_op
    : public boost::asio::coroutine
{
    using stream_t = boost::asio::ssl::stream<
        boost::asio::ip::tcp::socket>;
    stream_t& stream_;
    boost::asio::ip::tcp::resolver resolver_;
    std::string host_;
    std::string port_;

public:
    connect_op(
        stream_t& stream,
        boost::urls::url_view url) noexcept
        : stream_(stream)
        , resolver_(stream_.get_executor())
        , host_(url.host())
        , port_(url.has_port() ? url.port() : url.scheme())
    {
    }

    // Start
    template<class Self>
    void
    operator()(Self& self)
    {
        resolver_.async_resolve(
            host_, port_, std::move(self));
    }

    // On resolve
    template<class Self>
    void
    operator()(
        Self& self,
        boost::system::error_code ec,
        boost::asio::ip::tcp::resolver::results_type results)
    {
        if(ec)
            return self.complete(ec);

        boost::asio::async_connect(
            stream_.next_layer(), results, std::move(self));
    }

    // On connect
    template<class Self>
    void
    operator()(
        Self& self,
        boost::system::error_code ec,
        boost::asio::ip::tcp::resolver::results_type::endpoint_type)
    {
        if(ec)
            return self.complete(ec);

        if(!SSL_set_tlsext_host_name(
            stream_.native_handle(), host_.c_str()))
        {
            return self.complete({
                static_cast<int>(::ERR_get_error()),
                boost::asio::error::get_ssl_category()});
        }
        stream_.set_verify_callback(
            boost::asio::ssl::host_name_verification(host_));

        stream_.async_handshake(
            boost::asio::ssl::stream_base::client,
            std::move(self));
    }

    // On handshake
    template<class Self>
    void
    operator()(
        Self& self,
        boost::system::error_code ec)
    {
        self.complete(ec);
    }
};

template<
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code))
        CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::system::error_code))
client::
async_connect(CompletionToken&& token)
{
    return boost::asio::async_compose<
        CompletionToken,
        void(boost::system::error_code)>(
            connect_op(next_layer(), url_),
            token,
            next_layer());
}

} // jsonrpc

#endif
