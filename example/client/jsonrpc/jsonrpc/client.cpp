//
// Copyright (c) 2025 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include "client.hpp"

#include <boost/asio/append.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/coroutine.hpp>
#include <boost/asio/immediate.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast2.hpp>
#include <boost/capy/ex/system_context.hpp>
#include <boost/http/string_body.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/http/brotli/decode.hpp>
#include <boost/http/zlib/inflate.hpp>
#include <boost/url/url_view.hpp>

using namespace boost;

namespace jsonrpc {

namespace {

class stream_impl : public any_stream
{
    asio::ssl::stream<asio::ip::tcp::socket> stream_;
    bool secure_ = false;

public:
    stream_impl(
        asio::any_io_executor exec,
        asio::ssl::context& ssl_ctx)
        : stream_(exec, ssl_ctx)
    {
    }

    asio::any_io_executor
    get_executor() override
    {
        return stream_.get_executor();
    }

    void
    async_connect(
        urls::url_view endpoint,
        asio::any_completion_handler<void(system::error_code)> handler) override
    {
        secure_ = (endpoint.scheme_id() == urls::scheme::https);

        asio::async_compose<
            decltype(handler),
            void(system::error_code)>(
                connect_op(stream_, endpoint),
                handler,
                stream_);
    }

    void
    async_shutdown(
        asio::any_completion_handler<void(system::error_code)> handler) override
    {
        if(secure_)
        {
            stream_.async_shutdown(std::move(handler));
        }
        else
        {
            asio::async_compose<decltype(handler), void(system::error_code)>(
                tcp_shutdown_op{ stream_.next_layer() },
                handler,
                stream_);
        }
    }

    void
    async_write_some(
        const boost::span<boost::capy::const_buffer const>& buffers,
        asio::any_completion_handler<void(system::error_code, std::size_t)>
            handler) override
    {
        if(secure_)
            stream_.async_write_some(buffers, std::move(handler));
        else
            stream_.next_layer().async_write_some(buffers, std::move(handler));
    }

    void
    async_read_some(
        const boost::span<boost::capy::mutable_buffer const>& buffers,
        asio::any_completion_handler<void(system::error_code, std::size_t)>
            handler) override
    {
        if(secure_)
            stream_.async_read_some(buffers, std::move(handler));
        else
            stream_.next_layer().async_read_some(buffers, std::move(handler));
    }

private:
    class connect_op : public asio::coroutine
    {
        asio::ssl::stream<asio::ip::tcp::socket>& stream_;
        asio::ip::tcp::resolver resolver_;
        std::string host_;
        std::string port_;
        bool needs_handshake_;

    public:
        connect_op(
            asio::ssl::stream<asio::ip::tcp::socket>& stream,
            urls::url_view endpoint) noexcept
            : stream_(stream)
            , resolver_(stream_.get_executor())
            , host_(endpoint.host())
            , port_(endpoint.has_port() ? endpoint.port() : endpoint.scheme())
            , needs_handshake_(endpoint.scheme_id() == urls::scheme::https)
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
            system::error_code ec,
            asio::ip::tcp::resolver::results_type results)
        {
            if(ec)
                return self.complete(ec);

            asio::async_connect(
                stream_.next_layer(), results, std::move(self));
        }

        // On connect
        template<class Self>
        void
        operator()(
            Self& self,
            system::error_code ec,
            asio::ip::tcp::resolver::results_type::endpoint_type)
        {
            if(ec)
                return self.complete(ec);

            if(!needs_handshake_)
                return self.complete({});

            if(!SSL_set_tlsext_host_name(
                stream_.native_handle(), host_.c_str()))
            {
                return self.complete({
                    static_cast<int>(::ERR_get_error()),
                    asio::error::get_ssl_category()});
            }
            stream_.set_verify_callback(
                asio::ssl::host_name_verification(host_));

            stream_.async_handshake(
                asio::ssl::stream_base::client,
                std::move(self));
        }

        // On handshake
        template<class Self>
        void
        operator()(
            Self& self,
            system::error_code ec)
        {
            self.complete(ec);
        }
    };

    class tcp_shutdown_op
    {
        asio::ip::tcp::socket& socket_;

    public:
        tcp_shutdown_op(asio::ip::tcp::socket& socket)
            : socket_(socket)
        {
        }

        template<typename Self>
        void
        operator()(Self&& self)
        {
            system::error_code ec;
            socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            if(!ec)
                socket_.close(ec);
            asio::async_immediate(
                socket_.get_executor(),
                asio::append(std::move(self), ec));
        }

        template<typename Self>
        void
        operator()(Self&& self, system::error_code ec)
        {
            self.complete(ec);
        }
    };
};

class json_sink : public http::sink
{
    json::stream_parser& jpr_;

public:
    json_sink(json::stream_parser& jpr)
        : jpr_(jpr)
    {
    }

private:
    results
    on_write(
        capy::const_buffer b,
        bool more) override
    {
        results ret;
        ret.bytes = jpr_.write(
            static_cast<const char*>(b.data()),
            b.size(),
            ret.ec);
        if(!ret.ec && !more)
            jpr_.finish(ret.ec);
        return ret;
    }
};

} //namespace

// ----------------------------------------------

class client::request_op
    : public asio::coroutine
{
    client& client_;
    std::uint64_t id_;
    std::string body_;

public:
    request_op(client& c) noexcept
        : client_(c)
        , id_(c.next_id())
    {
    }

    template<typename Self>
    void
    operator()(
        Self&& self,
        boost::core::string_view method,
        boost::json::value params)
    {
        auto sp = params.storage();
        json::object value(
            {
                { "jsonrpc", "2.0" },
                { "method", method },
                { "params", std::move(params)},
                { "id", id_ }
            },
            sp);
        body_ = json::serialize(value);
        client_.req_.set_content_length(body_.size());
        client_.sr_.start(
            client_.req_, http::string_body(std::move(body_)));

        beast2::async_write(
            *client_.stream_, client_.sr_, std::move(self));
    }

    template<class Self>
    void
    operator()(
        Self& self,
        system::error_code ec = {},
        std::size_t = 0)
    {
        if(ec)
            return self.complete(ec, {});

        BOOST_ASIO_CORO_REENTER(*this)
        {
            client_.pr_.start();
            BOOST_ASIO_CORO_YIELD
            beast2::async_read_header(
                *client_.stream_, client_.pr_, std::move(self));

            client_.pr_.set_body<json_sink>(client_.jpr_);
            BOOST_ASIO_CORO_YIELD
            beast2::async_read(
                *client_.stream_, client_.pr_, std::move(self));

            {
                auto body = client_.jpr_.release();
                if(!body.is_object())
                    return self.complete({ errc::invalid_response }, {});
                auto& obj = body.get_object();
                auto* ver = obj.if_contains("jsonrpc");
                auto* err = obj.if_contains("error");
                auto* res = obj.if_contains("result");
                auto* id  = obj.if_contains("id");
                if(!ver || *ver != "2.0")
                    return self.complete({ errc::version_error }, {});
                if(err && err->is_object())
                {
                    auto* code = err->get_object().if_contains("code");
                    if(!code || !code->is_int64())
                        return self.complete({ errc::invalid_response }, {});
                    switch (code->get_int64())
                    {
                    case -32700: ec = errc::parse_error;      break;
                    case -32600: ec = errc::invalid_request;  break;
                    case -32601: ec = errc::method_not_found; break;
                    case -32602: ec = errc::invalid_params;   break;
                    case -32603: ec = errc::internal_error;   break;
                    default:     ec = errc::server_error;     break;
                    }
                    return self.complete({ ec, std::move(err->get_object()) }, {});
                }
                else if(res)
                {
                    if(!id || *id != id_)
                        return self.complete({ errc::id_mismatch }, {});

                    return self.complete({}, std::move(*res));
                }
                self.complete({ errc::invalid_response }, {});
            }
        }
    }
};

// ----------------------------------------------

client::
client(
    urls::url endpoint,
    asio::any_io_executor exec,
    asio::ssl::context& ssl_ctx)
    : client(
        std::move(endpoint),
        std::unique_ptr<any_stream>(new stream_impl(exec, ssl_ctx)))
{
}

client::
client(
    urls::url endpoint,
    std::unique_ptr<any_stream> stream)
    : stream_(std::move(stream))
    , endpoint_(std::move(endpoint))
    , req_(http::method::post, "/")
{
    using field = http::field;

    if(!endpoint_.encoded_target().empty())
        req_.set_target(endpoint_.encoded_target());

    req_.append(field::host, endpoint_.encoded_host_and_port().decode());
    req_.append(field::content_type, "application/json");
    req_.append(field::accept, "application/json");
    req_.append(field::user_agent, "Boost.Http.Io");

    auto& ctx = capy::get_system_context();
    if(ctx.find_service<http::brotli::decode_service>() != nullptr)
        req_.append(field::accept_encoding, "br");

    if(ctx.find_service<http::zlib::inflate_service>() != nullptr)
        req_.append(field::accept_encoding, "deflate, gzip");

    pr_.reset();
}

void 
client::
async_call_impl(
    asio::any_completion_handler<void(error, json::value)> handler,
    core::string_view method,
    json::value params)
{
    return boost::asio::async_initiate<decltype(handler), void(error, json::value)>(
        boost::asio::composed<void(error, json::value)>(
            client::request_op(*this),
            *stream_),
        handler,
        method,
        std::move(params));
}

} // jsonrpc
