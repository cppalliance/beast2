//
// Copyright (c) 2025 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef JSONRPC_CLIENT_HPP
#define JSONRPC_CLIENT_HPP

#include "any_stream.hpp"
#include "detail/converts_to.hpp"
#include "detail/initiation_base.hpp"
#include "error.hpp"
#include "method.hpp"

#include <boost/asio/ssl/context.hpp>
#include <boost/http_proto/request.hpp>
#include <boost/http_proto/response_parser.hpp>
#include <boost/http_proto/serializer.hpp>
#include <boost/json/value_to.hpp>
#include <boost/rts/context_fwd.hpp>
#include <boost/url/url.hpp>

namespace jsonrpc {

class client
{
    std::unique_ptr<any_stream> stream_;
    boost::urls::url endpoint_;
    boost::http_proto::serializer sr_;
    boost::http_proto::response_parser pr_;
    boost::http_proto::request req_;
    std::uint64_t id_ = 0;
    struct invoker_base
    {
        friend client;
        client* self_;
        boost::core::string_view method_;
        invoker_base(client*, boost::core::string_view);
    };

public:
    /// The type of the next layer.
    using next_layer_type = any_stream;

    /// The type of the executor associated with the object.
    using executor_type = typename next_layer_type::executor_type;

    client(
        boost::urls::url endpoint,
        const boost::rts::context& rts_ctx,
        boost::asio::any_io_executor exec,
        boost::asio::ssl::context& ssl_ctx);

    client(
        boost::urls::url endpoint,
        const boost::rts::context& rts_ctx,
        std::unique_ptr<any_stream> stream);

    executor_type
    get_executor() noexcept
    {
        return stream_->get_executor();
    }

    next_layer_type&
    next_layer() noexcept
    {
        return *stream_;
    }

    next_layer_type const&
    next_layer() const noexcept
    {
        return *stream_;
    }

    boost::urls::url_view
    endpoint() const noexcept
    {
        return endpoint_;
    }

    boost::http_proto::fields_base&
    http_fields()
    {
        return req_;
    }

    template<
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code))
            CompletionToken = boost::asio::deferred_t>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::system::error_code))
    async_connect(CompletionToken&& token = {})
    {
        return boost::asio::async_initiate<
            CompletionToken, void(boost::system::error_code)>(
                initiate_async_connect(get_executor()),
                token,
                this);
    }

    template<
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(boost::system::error_code))
            CompletionToken = boost::asio::deferred_t>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(boost::system::error_code))
    async_shutdown(CompletionToken&& token = {})
    {
        return boost::asio::async_initiate<
            CompletionToken, void(boost::system::error_code)>(
                initiate_async_shutdown(get_executor()),
                token,
                this);
    }

    template<typename Signature>
    class invoker;

    /** Calls a remote procedure that takes no parameters.

        Instances of this type are returned from @ref operator[].

        @tparam Return The return type of the procedure.
    */
    template<typename Return>
    class invoker<Return()> : invoker_base
    {
        using invoker_base::invoker_base;
    public:

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

    /** Calls a remote procedure with positional parameters (as an array).

        Instances of this type are returned from @ref operator[].

        @tparam Return The return type of the procedure.
    */
    template<typename Return>
    class invoker<Return(boost::json::array)> : invoker_base
    {
        using invoker_base::invoker_base;
    public:

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

    /** Calls a remote procedure with named parameters (as an object).

        Instances of this type are returned from @ref operator[].

        @tparam Return The return type of the procedure.
    */
    template<typename Return>
    class invoker<Return(boost::json::object)>: invoker_base
    {
        using invoker_base::invoker_base;
    public:

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
    class request_op;

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

    class initiate_async_connect : public detail::initiation_base
    {
    public:
        using detail::initiation_base::initiation_base;

        template<typename Handler>
        void
        operator()(Handler&& handler, client* self)
        {
            self->stream_->async_connect(
                self->endpoint_, std::move(handler));
        }
    };

    class initiate_async_shutdown : public detail::initiation_base
    {
    public:
        using detail::initiation_base::initiation_base;

        template<typename Handler>
        void
        operator()(Handler&& handler, client* self)
        {
            self->stream_->async_shutdown(std::move(handler));
        }
    };

    class initiate_async_request : public detail::initiation_base
    {
    public:
        using detail::initiation_base::initiation_base;

        template<typename Handler>
        void
        operator()(
            Handler&& handler,
            boost::core::string_view method,
            boost::json::value params,
            client* self)
        {
            self->async_request_impl(
                std::move(handler), method, std::move(params));
        }
    };

    void async_request_impl(
        boost::asio::any_completion_handler<void(error, boost::json::value)> handler,
        boost::core::string_view method,
        boost::json::value params);

    template<typename CompletionToken>
    auto
    async_request(
        boost::core::string_view method,
        boost::json::value params,
        CompletionToken&& token)
        -> decltype(boost::asio::async_initiate<
            CompletionToken, void(error, boost::json::value)>(
                initiate_async_request(get_executor()),
                token,
                method,
                std::move(params),
                this)
        )
    {
        return boost::asio::async_initiate<
            CompletionToken, void(error, boost::json::value)>(
                initiate_async_request(get_executor()),
                token,
                method,
                std::move(params),
                this);
    }

    std::uint64_t
    next_id() noexcept
    {
        return ++id_;
    }
};

} // jsonrpc

#endif
