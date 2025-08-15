//
// Copyright (c) 2025 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_EXAMPLE_CLIENT_JSONRPC_LIB_BASIC_CLIENT_HPP
#define BOOST_HTTP_IO_EXAMPLE_CLIENT_JSONRPC_LIB_BASIC_CLIENT_HPP

#include "error.hpp"
#include "method.hpp"

#include <boost/asio/compose.hpp>
#include <boost/http_io.hpp>
#include <boost/http_proto/parser.hpp>
#include <boost/http_proto/request.hpp>
#include <boost/http_proto/serializer.hpp>
#include <boost/http_proto/string_body.hpp>
#include <boost/json.hpp>
#include <boost/rts/context_fwd.hpp>
#include <boost/url/url_view.hpp>

namespace jsonrpc {

template<class Stream>
class basic_client
{
    Stream stream_;
    boost::http_proto::serializer sr_;
    boost::http_proto::response_parser pr_;
    boost::http_proto::request req_;
    boost::json::parser jpr_;
    std::uint64_t id_ = 0;

    template<typename Return>
    class request_op;

public:
    /// The type of the next layer.
    using next_layer_type =
        typename std::remove_reference<Stream>::type;

    /// The type of the executor associated with the object.
    using executor_type = typename next_layer_type::executor_type;

    template<typename... Args>
    basic_client(
        boost::urls::url_view url,
        const boost::rts::context& rts_ctx,
        Args&&... args)
        : stream_(std::forward<Args>(args)...)
        , sr_(rts_ctx)
        , pr_(rts_ctx)
        , req_(boost::http_proto::method::post, "/")
    {
        using field = boost::http_proto::field;
        if(!url.encoded_target().empty())
            req_.set_target(url.encoded_target());
        req_.set(field::host, url.encoded_host_and_port().decode());
        req_.set(field::content_type, "application/json");
        req_.set(field::accept, "application/json");
        req_.set(field::user_agent, "Boost.Http.Io");
        pr_.reset();
    }

    executor_type
    get_executor() noexcept
    {
        return stream_.get_executor();
    }

    next_layer_type&
    next_layer() noexcept
    {
        return stream_;
    }

    next_layer_type const&
    next_layer() const noexcept
    {
        return stream_;
    }

    boost::http_proto::fields_base&
    http_fields()
    {
        return req_;
    }

    template<typename Signature>
    struct invoker;

    template<typename Return>
    struct invoker<Return()>
    {
        method<Return()> m;
        basic_client& c;

        template<
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error, Return))
                CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
        BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error, Return))
        operator()(
            CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));
    };

    template<typename Return>
    struct invoker<Return(boost::json::array)>
    {
        method<Return(boost::json::array)> m;
        basic_client& c;

        template<
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error, Return))
                CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
        BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error, Return))
        operator()(
            const boost::json::array& params,
            CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));
    };

    template<typename Return>
    struct invoker<Return(boost::json::object)>
    {
        method<Return(boost::json::object)> m;
        basic_client& c;

        template<
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error, Return))
                CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
        BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error, Return))
        operator()(
            const boost::json::object& params,
            CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type));
    };

    template<typename Signature>
    invoker<Signature>
    operator[](method<Signature> m) noexcept
    {
        return { m, *this };
    }

private:
    std::uint64_t
    next_id() noexcept
    {
        return ++id_;
    }

    std::string
    make_body(
        boost::core::string_view method,
        const boost::json::value& params,
        std::uint64_t id)
    {
        // TODO: serialize params without copying
        return boost::json::serialize(
            boost::json::object(
                {
                    { "jsonrpc", "2.0" },
                    { "method", method },
                    { "params", params },
                    { "id", id }
                },
                params.storage()));
    }
};

template<class Stream>
template<typename Return>
class basic_client<Stream>::request_op
    : public boost::asio::coroutine
{
    basic_client& c_;
    std::string body_;
    std::uint64_t id_;

public:
    request_op(
        basic_client& c,
        std::string body,
        std::uint64_t id) noexcept
        : c_(c)
        , body_(std::move(body))
        , id_(id)
    {
    }

    template<class Self>
    void
    operator()(
        Self& self,
        boost::system::error_code ec = {},
        std::size_t = 0)
    {
        if(ec)
            return self.complete(ec, {});

        BOOST_ASIO_CORO_REENTER(*this)
        {
            c_.req_.set_payload_size(body_.size());
            c_.sr_.template start<
                boost::http_proto::string_body>(
                    c_.req_, std::move(body_));

            BOOST_ASIO_CORO_YIELD
            boost::http_io::async_write(
                c_.stream_, c_.sr_, std::move(self));

            c_.pr_.start();
            BOOST_ASIO_CORO_YIELD
            boost::http_io::async_read_header(
                c_.stream_, c_.pr_, std::move(self));

            BOOST_ASIO_CORO_YIELD
            boost::http_io::async_read(
                c_.stream_, c_.pr_, std::move(self));

            {
                auto body = boost::json::parse(c_.pr_.body(), ec);
                if(ec || !body.is_object())
                    return self.complete({ errc::invalid_response }, {});
                auto& obj = body.as_object();
                auto* ver = obj.if_contains("jsonrpc");
                auto* err = obj.if_contains("error");
                auto* res = obj.if_contains("result");
                auto* id  = obj.if_contains("id");
                if(!ver || *ver != "2.0")
                    return self.complete({ errc::version_error }, {});
                if(!id || *id != id_)
                    return self.complete({ errc::id_mismatch }, {});
                if(err && err->is_object())
                {
                    auto* code = err->as_object().if_contains("code");
                    if(!code || !code->is_int64())
                        return self.complete({ errc::invalid_response }, {});
                    switch (code->as_int64())
                    {
                    case -32700: ec = errc::parse_error;      break;
                    case -32600: ec = errc::invalid_request;  break;
                    case -32601: ec = errc::method_not_found; break;
                    case -32602: ec = errc::invalid_params;   break;
                    case -32603: ec = errc::internal_error;   break;
                    default:     ec = errc::server_error;     break;
                    }
                    return self.complete({ ec, std::move(err->as_object()) }, {});
                }
                else if(res)
                {
                    // TODO: avoid extra copy
                    return self.complete({}, boost::json::value_to<Return>(*res));
                }
                self.complete({ errc::invalid_response }, {});
            }
        }
    }
};

template<class Stream>
template<typename Return>
template<
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error, Return))
        CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error, Return))
basic_client<Stream>::
invoker<Return()>::
operator()(
    CompletionToken&& token)
{
    auto id = c.next_id();
    return boost::asio::async_compose<
        CompletionToken,
        void(error, Return)>(
            basic_client::request_op<Return>(c, c.make_body(m.name, {}, id), id),
            token,
            c.stream_);
}

template<class Stream>
template<typename Return>
template<
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error, Return))
        CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error, Return))
basic_client<Stream>::
invoker<Return(boost::json::array)>::
operator()(
    const boost::json::array& params,
    CompletionToken&& token)
{
    auto id = c.next_id();
    return boost::asio::async_compose<
        CompletionToken,
        void(error, Return)>(
            basic_client::request_op<Return>(c, c.make_body(m.name, params, id), id),
            token,
            c.stream_);
}

template<class Stream>
template<typename Return>
template<
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error, Return))
        CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error, Return))
basic_client<Stream>::
invoker<Return(boost::json::object)>::
operator()(
    const boost::json::object& params,
    CompletionToken&& token)
{
    auto id = c.next_id();
    return boost::asio::async_compose<
        CompletionToken,
        void(error, Return)>(
            basic_client::request_op<Return>(c, c.make_body(m.name, params, id), id),
            token,
            c.stream_);
}

} // jsonrpc

#endif
