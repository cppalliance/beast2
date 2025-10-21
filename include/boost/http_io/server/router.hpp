//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_SERVER_ROUTER_HPP
#define BOOST_HTTP_IO_SERVER_ROUTER_HPP

#include <boost/http_io/detail/config.hpp>
#include <boost/http_io/server/any_lambda.hpp>
#include <boost/http_proto/method.hpp>
#include <boost/url/segments_encoded_view.hpp>
#include <boost/core/detail/string_view.hpp>
#include <memory>

namespace boost {
namespace http_io {

struct Request;

//------------------------------------------------

#if 0
template<class T>
struct of_type_t
{
    using type = T;
};

namespace detail {
template<class T>
struct of_type_impl
{
    static of_type_t<T> const value;
};
template<class T>
of_type_t<T> const
of_type_impl<T>::value = of_type_t<T>{};
} // detail

template<class T>
constexpr of_type_t<T> const& of_type =
    detail::of_type_impl<T>::value;
#endif

//------------------------------------------------

template<class Response, class Request>
class router;

//------------------------------------------------

class router_base
{
protected:
    struct BOOST_SYMBOL_VISIBLE
        any_handler
    {
        BOOST_HTTP_IO_DECL
        virtual ~any_handler();

        virtual bool operator()(
            void* req, void* res) const = 0;
    };

    using handler_ptr = std::unique_ptr<any_handler>;

    BOOST_HTTP_IO_DECL
    router_base(
        http_proto::method(*)(void*),
        urls::segments_encoded_view&(*)(void*));

    BOOST_HTTP_IO_DECL
    void use(handler_ptr);

    BOOST_HTTP_IO_DECL
    void insert(
        http_proto::method,
        core::string_view,
        handler_ptr);

    BOOST_HTTP_IO_DECL
    bool invoke(void*, void*) const;

    struct entry;
    struct impl;

    std::shared_ptr<impl> impl_;
};

//------------------------------------------------

/** A container mapping HTTP requests to handlers
*/
template<
    class Response,
    class Request = http_io::Request>
class router : public router_base
{
public:
    router()
        : router_base(
            [](void* req) -> http_proto::method
            {
                return reinterpret_cast<Request*>(req)->method;
            },
            [](void* req) -> urls::segments_encoded_view&
            {
                return reinterpret_cast<Request*>(req)->path;
            })
    {
    }

    /** Add a global handler
    */
    template<class Handler>
    router_base&
    use(Handler&& h)
    {
        router_base::use(handler_ptr(
            new handler_impl<Handler>(
                std::forward<Handler>(h))));
        return *this;
    }

    /** Add a GET handler
    */
    template<class Handler>
    void
    get(
        core::string_view pattern,
        Handler&& h)
    {
        insert(http_proto::method::get,
            pattern, std::forward<Handler>(h));
    }

    /** Add a handler to match a method and pattern
    */
    template<class Handler>
    void
    insert(
        http_proto::method method,
        core::string_view pattern,
        Handler&& h)
    {
        router_base::insert(method, pattern,
            handler_ptr(new handler_impl<Handler>(
                std::forward<Handler>(h))));
    }

    bool operator()(
        Request& req, Response& res) const
    {
        return invoke(&req, &res);
    }

private:
    template<class Handler>
    struct handler_impl : any_handler
    {
        template<class... Args>
        handler_impl(Args&&... args)
            : h(std::forward<Args>(args)...)
        {
        }

    private:
        using handler_type = typename
            std::decay<Handler>::type;
        handler_type h;
        //static_assert(std::is_invocable<
            //Handler, Response&, Request&>::value, "");
        bool operator()(void* req, void* res) const override
        {
            return h(
                *reinterpret_cast<Request*>(req),
                *reinterpret_cast<Response*>(res));
        }
    };
};

//------------------------------------------------

} // http_io
} // boost

#endif
