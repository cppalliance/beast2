//
// Copyright (c) 2025 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef JSONRPC_CLIENT_HPP
#define JSONRPC_CLIENT_HPP

#include "any_stream.hpp"
#include "detail/converts_to.hpp"
#include "detail/initiation_base.hpp"
#include "error.hpp"
#include "method.hpp"

#include <boost/asio/compose.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/http_proto/request.hpp>
#include <boost/http_proto/response_parser.hpp>
#include <boost/http_proto/serializer.hpp>
#include <boost/json/serializer.hpp>
#include <boost/json/stream_parser.hpp>
#include <boost/json/value_to.hpp>
#include <boost/capy/polystore_fwd.hpp>
#include <boost/url/url.hpp>

namespace jsonrpc {

/// A JSON-RPC 2.0 client.
class client
{
    std::unique_ptr<any_stream> stream_;
    boost::urls::url endpoint_;
    boost::http_proto::serializer sr_;
    boost::http_proto::response_parser pr_;
    boost::http_proto::request req_;
    boost::json::stream_parser jpr_;
    boost::json::serializer jsr_;
    std::uint64_t id_ = 0;

public:
    /// The type of the next layer.
    using next_layer_type = any_stream;

    /// The type of the executor associated with the object.
    using executor_type = typename any_stream::executor_type;

    /** Constructor.

        Constructs a client capable of connecting to
        an HTTP or HTTPS endpoint.
    */ 
    client(
        boost::urls::url endpoint,
        boost::capy::polystore& capy_ctx,
        boost::asio::any_io_executor exec,
        boost::asio::ssl::context& ssl_ctx);

    /** Constructor.

        Constructs a client using the supplied
        @ref any_stream instance.
    */ 
    client(
        boost::urls::url endpoint,
        boost::capy::polystore& capy_ctx,
        std::unique_ptr<any_stream> stream);

    /// Get the executor associated with the object.
    executor_type
    get_executor() noexcept
    {
        return stream_->get_executor();
    }

    /// Get a reference to the next layer.
    next_layer_type&
    next_layer() noexcept
    {
        return *stream_;
    }

    /// Get a reference to the next layer.
    next_layer_type const&
    next_layer() const noexcept
    {
        return *stream_;
    }

    /// Return the endpoint that the client is configured with.
    boost::urls::url_view
    endpoint() const noexcept
    {
        return endpoint_;
    }

    /** Return a reference to the fields container of the HTTP
        request message.

        This function can be used to customize HTTP headers, for
        example, to add the required credentials.
    */
    boost::http_proto::fields_base&
    http_fields()
    {
        return req_;
    }

    /// Connect to the endpoint.
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

    /// Shutdown the stream.
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

    /** Call a remote procedure that takes no parameters.

        The result type depends on the signature of the passed @ref method.

        @param m The @ref method object that contains the name
        and signature of the remote procedure.

        @param token The completion token used to produce a completion
        handler, which will be invoked when the call completes.
    */
    template<
        typename Return,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error, Return))
            CompletionToken = boost::asio::deferred_t>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error, Return))
    async_call(
        method<Return()> method,
        CompletionToken&& token = {})
    {
        return async_call_impl<Return>(method.name, {}, std::move(token));
    }

    /** Call a remote procedure with positional parameters (array).

        The result type depends on the signature of the passed @ref method.

        @param m The @ref method object that contains the name
        and signature of the remote procedure.

        @param params A JSON array containing the positional parameters
        to be used when calling the server method.

        @param token The completion token used to produce a completion
        handler, which will be invoked when the call completes.
    */
    template<
        typename Return,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error, Return))
            CompletionToken = boost::asio::deferred_t>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error, Return))
    async_call(
        method<Return(boost::json::array)> method,
        boost::json::array params,
        CompletionToken&& token = {})
    {
        return async_call_impl<Return>(
            method.name, std::move(params), std::move(token));
    }

    /** Call a remote procedure with named parameters (object).

        The result type depends on the signature of the passed @ref method.

        @param m The @ref method object that contains the name
        and signature of the remote procedure.

        @param params A JSON object containing the named parameters
        to be used when calling the server method.

        @param token The completion token used to produce a completion
        handler, which will be invoked when the call completes.
    */
    template<
        typename Return,
        BOOST_ASIO_COMPLETION_TOKEN_FOR(void(error, Return))
            CompletionToken = boost::asio::deferred_t>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error, Return))
    async_call(
        method<Return(boost::json::object)> method,
        boost::json::object params,
        CompletionToken&& token = {})
    {
        return async_call_impl<Return>(
            method.name, std::move(params), std::move(token));
    }

private:
    class request_op;

    template<typename Return>
    class request_and_transform_op
    {
        client& client_;

    public:
        request_and_transform_op(client& c) noexcept
            : client_(c)
        {
        }

        template<typename Self>
        void
        operator()(
            Self&& self,
            boost::core::string_view method,
            boost::json::value params)
        {
            client_.async_call_impl(
                std::move(self), method, std::move(params));
        }

        template<typename Self>
        void
        operator()(Self&& self, error e, boost::json::value v)
        {
            if(e.code())
                return self.complete(std::move(e), {});

            // avoid extra copies when possible
            if(auto* o = detail::converts_to<Return>(v))
                return self.complete(std::move(e), std::move(*o));

            auto o = boost::json::try_value_to<Return>(v);
            if(o.has_error())
                return self.complete(o.error(), {});

            self.complete(std::move(e), std::move(*o));
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

    void async_call_impl(
        boost::asio::any_completion_handler<void(error, boost::json::value)> handler,
        boost::core::string_view method,
        boost::json::value params);

    template<typename Return, typename CompletionToken>
    BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(error, Return))
    async_call_impl(
        boost::core::string_view method,
        boost::json::value params,
        CompletionToken&& token)
    {
        return boost::asio::async_initiate<CompletionToken, void(error, Return)>(
            boost::asio::composed<void(error, Return)>(
                request_and_transform_op<Return>(*this),
                *stream_),
            token,
            method,
            std::move(params));
    }

    std::uint64_t
    next_id() noexcept
    {
        return ++id_;
    }
};

} // jsonrpc

#endif
