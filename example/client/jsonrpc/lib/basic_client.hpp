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

#include "detail/converts_to.hpp"
#include "error.hpp"
#include "method.hpp"

#include <boost/asio/any_completion_handler.hpp>
#include <boost/asio/compose.hpp>
#include <boost/http_io.hpp>
#include <boost/http_proto/parser.hpp>
#include <boost/http_proto/request.hpp>
#include <boost/http_proto/serializer.hpp>
#include <boost/http_proto/string_body.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
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
    std::uint64_t id_ = 0;
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

    class invoker_base
    {
    protected:
        basic_client* self_;
        boost::core::string_view method_;
    public:
        invoker_base(
            basic_client* self,
            boost::core::string_view method)
            : self_(self)
            , method_(method)
        {
        }
    };

    template<typename Signature>
    class invoker;

    /** Calls a remote procedure that takes no parameters.

        @tparam Return the return type of procedure.
    */
    template<typename Return>
    class invoker<Return()> : public invoker_base
    {
    public:
        using invoker_base::invoker_base;
        template<
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error, Return))
                CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
        BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error, Return))
        operator()(
            CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
        {
            return this->self_->async_request(
                this->method_,
                {},
                boost::asio::deferred(transform<Return>{})) | std::move(token);
        }
    };

    /** Calls a remote procedure with positional parameters (array).

        @tparam Return the return type of procedure.
    */
    template<typename Return>
    class invoker<Return(boost::json::array)>
        : public invoker_base
    {
    public:
        using invoker_base::invoker_base;

        template<
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error, Return))
                CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
        BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error, Return))
        operator()(
            boost::json::array params,
            CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
        {
            return this->self_->async_request(
                this->method_,
                std::move(params),
                boost::asio::deferred(transform<Return>{})) | std::move(token);
        }
    };

    /** Calls a remote procedure with named parameters (object).

        @tparam Return the return type of procedure.
    */
    template<typename Return>
    class invoker<Return(boost::json::object)>
        : public invoker_base
    {
    public:
        using invoker_base::invoker_base;

        template<
            BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error, Return))
                CompletionToken BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
        BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error, Return))
        operator()(
            boost::json::object params,
            CompletionToken&& token BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
        {
            return this->self_->async_request(
                this->method_,
                std::move(params),
                boost::asio::deferred(transform<Return>{})) | std::move(token);
        }
    };

    template<typename Signature>
    invoker<Signature>
    operator[](method<Signature> m) noexcept
    {
        return { this, m.name };
    }

private:
    template<typename Return>
    class transform
    {
    public:
        boost::asio::deferred_values<error, Return>
        operator()(error e, boost::json::value v)
        {
            constexpr auto deferred = boost::asio::deferred;
            if(e.code())
                return deferred.values(std::move(e), Return{});

            // avoid extra copies when possible
            if(auto* o = detail::converts_to<Return>(v))
                return deferred.values(std::move(e), std::move(*o));

            auto o = boost::json::try_value_to<Return>(v);
            if(o.has_error())
                return deferred.values(error(o.error()), Return{});
            return deferred.values(std::move(e), std::move(*o));
        }
    };

    class request_op
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
                    auto& obj = body.get_object();
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
                        return self.complete({}, std::move(*res));
                    }
                    self.complete({ errc::invalid_response }, {});
                }
            }
        }
    };

    class initiate_async_request
    {
        basic_client* self_;

    public:
        initiate_async_request(basic_client* self) noexcept
            : self_(self)
        {
        }

        using executor_type = typename basic_client::executor_type;

        executor_type
        get_executor() noexcept
        {
            return self_->get_executor();
        }

        void
        operator()(
            boost::asio::any_completion_handler<void(error, boost::json::value)> handler,
            boost::core::string_view method,
            boost::json::value params)
        {
            auto id = self_->next_id();
            boost::asio::async_compose<
                decltype(handler),
                void(error, boost::json::value)>(
                    basic_client::request_op(*self_, self_->make_body(method, params, id), id),
                    handler,
                    self_->stream_);
        }
    };

    template<typename CompletionToken>
    auto
    async_request(
        boost::core::string_view method,
        boost::json::value params,
        CompletionToken&& token)
        -> decltype(boost::asio::async_initiate<
            CompletionToken, void(error, boost::json::value)>(
                initiate_async_request(this),
                token,
                method,
                std::move(params))
        )
    {
        return boost::asio::async_initiate<
            CompletionToken, void(error, boost::json::value)>(
                initiate_async_request(this),
                token,
                method,
                std::move(params));
    }

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

} // jsonrpc

#endif
