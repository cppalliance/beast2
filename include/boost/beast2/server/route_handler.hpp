//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_ROUTE_HANDLER_HPP
#define BOOST_BEAST2_SERVER_ROUTE_HANDLER_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/server/router_types.hpp>
#include <boost/capy/polystore.hpp>
#include <boost/http_proto/request.hpp>  // VFALCO forward declare?
#include <boost/http_proto/request_parser.hpp>  // VFALCO forward declare?
#include <boost/http_proto/response.hpp>        // VFALCO forward declare?
#include <boost/http_proto/serializer.hpp>      // VFALCO forward declare?
#include <boost/url/url_view.hpp>
#include <boost/system/error_code.hpp>
#include <memory>

namespace boost {
namespace beast2 {

struct acceptor_config
{
    bool is_ssl;
    bool is_admin;
};

//-----------------------------------------------

/** Request object for HTTP route handlers
*/
struct Request : basic_request
{
    /** The complete request target

        This is the parsed directly from the start
        line contained in the HTTP request and is
        never modified.
    */
    urls::url_view url;

    /** The HTTP request message
    */
    http_proto::request message;

    /** The HTTP request parser
        This can be used to take over reading the body.
    */
    http_proto::request_parser parser;

    /** A container for storing arbitrary data associated with the request.
        This starts out empty for each new request.
    */
    capy::polystore data;
};

//-----------------------------------------------

/** Response object for HTTP route handlers
*/
struct Response : basic_response
{
    /** The HTTP response message
    */
    http_proto::response message;

    /** The HTTP response serializer
    */
    http_proto::serializer serializer;

    /** The detacher for this session.
        This can be used to detach from the
        session and take over managing the
        connection.
    */
    detacher detach;

    /** A container for storing arbitrary data associated with the session.

        This starts out empty for each new session.
    */
    capy::polystore data;

    /** Reset the object for a new request.
        This clears any state associated with
        the previous request, preparing the object
        for use with a new request.
    */
    BOOST_BEAST2_DECL void reset();

#if 0
    route_result close() const noexcept
    {
        return route::close;
    }

    route_result complete() const noexcept
    {
        return route::complete;
    }

    route_result next() const noexcept
    {
        return route::next;
    }

    route_result next_route() const noexcept
    {
        return route::next_route;
    }

    route_result send() const noexcept
    {
        return route::send;
    }

    BOOST_BEAST2_DECL
    route_result
    fail(system::error_code const& ec);
#endif

    // route_result send(core::string_view);

    /** Set the status code of the response.
        @par Example
        @code
        res.status(http_proto::status::not_found);
        @endcode
        @param code The status code to set.
        @return A reference to this response.
    */
    BOOST_BEAST2_DECL
    Response&
    status(http_proto::status code);

    BOOST_BEAST2_DECL
    Response&
    set_body(std::string s);

#if 0
    /** Submit cooperative work.

        This function detaches the current handler from the session,
        and immediately invokes the specified function object @p f.
        When the function returns normally, the seesion is resumed as
        if the current handler had returned the same result. Otherwise, if
        the function invokes @ref post again then an implementation-defined
        yield operation is performed, possibly allowing other work to be
        performed on the calling thread. When the function object is invoked,
        it runs in the same context as the original handler invocation.

        The function object @p f must have this equivalent signature:
        @code
        route_result( Response& );
        @endcode

        The function must not return @ref route::detach, or else an
        exception is thrown.

        @throws std::invalid_argument If the function returns @ref route::detach.
    */
    template<class F>
    auto
    post(F&& f) -> route_result
    {
        struct model : work
        {
            using handler = typename std::decay<F>::type;
            handler f_;

            model(F&& f)
                : f_(std::forward<F>(f))
            {
            }

            route_result invoke(
                std::unique_ptr<work>& w,
                Response& res) override
            {
                handler f(std::move(f_));
                auto resume_ = this->resume;
                w.reset();
                return f(res);
            }
        };

        struct bool_guard
        {
            bool* pb_;

            bool_guard(bool& b) noexcept
                : pb_(&b)
            {
            }

            ~bool_guard()
            {
                if(pb_)
                    *pb_ = false;
            }

            void reset() noexcept
            {
                pb_ = nullptr;
            }
        };

        if(! in_work_)
        {
            // new work
            bool_guard g(in_work_);
            in_work_ = true;
            auto rv = f(*this);
            if(post_called_)
            {
                BOOST_ASSERT(rv == route::detach);
                return rv;
            }
            if(rv == route::detach)
                detail::throw_invalid_argument();
            return rv;
        }

        return detach(
            [&](resumer resume)
            {
                std::unique_ptr<work> p(
                    new model(std::forward<F>(f)));
                p->resume = resume;
            });
    }

protected:
    struct work
    {
        resumer resume;
        virtual ~work() = default;
        virtual route_result invoke(
            std::unique_ptr<work>&,
            Response&) = 0;
    };
    virtual route_result do_post(work& w)
    {
    }

private:
    bool in_work_ = false;
    bool post_called_ = false;
    work* work_ = nullptr;
#endif
};

} // beast2
} // boost

#endif
