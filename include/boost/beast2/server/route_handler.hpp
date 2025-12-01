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
#include <boost/capy/datastore.hpp>
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
    capy::datastore data;
};

//-----------------------------------------------

/** Response object for HTTP route handlers
*/
struct BOOST_SYMBOL_VISIBLE
    Response : basic_response
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
    capy::datastore data;

    /** Destructor.
    */
    BOOST_BEAST2_DECL virtual ~Response();

    /** Reset the object for a new request.
        This clears any state associated with
        the previous request, preparing the object
        for use with a new request.
    */
    BOOST_BEAST2_DECL void reset();

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

    // VFALCO this doc isn't quite right because it doesn't explain
    // the possibility that post will return the final result immediately,
    // and it needs to remind the user that calling a function which
    // returns route_result means they have to return the value right away
    // without doing anything else.
    //
    // VFALCO we have to detect calls to detach inside `f` and throw
    //
    /** Submit cooperative work.

        This function detaches the current handler from the session,
        and immediately invokes the specified function object @p f.
        When the function returns normally, the function object is
        placed into an implementation-defined work queue to be invoked
        again. Otherwise, if the function calls `resume(rv)` then the
        session is resumed and behaves as if the original route handler
        had returned the value `rv`.

        When the function object is invoked, it runs in the same context
        as the original handler invocation. If the function object
        attempts to call @ref post again, or attempts to call @ref detach,
        an exception is thrown.

        The function object @p f must have this equivalent signature:
        @code
        void ( resumer resume );
        @endcode

        @param f The function object to invoke.
        @param c The continuation function to be invoked when f finishes.
    */
    template<class F>
    auto
    post(F&& f) -> route_result;

protected:
    /** A task to be invoked later
    */
    struct task
    {
        virtual ~task() = default;

        /** Invoke the task.

            @return true if the task resumed the session.
        */
        virtual bool invoke() = 0;
    };

    /** Post task_ to be invoked later

        Subclasses must schedule task_ to be invoked at an unspecified
        point in the future.
    */
    BOOST_BEAST2_DECL
    virtual void do_post();

    std::unique_ptr<task> task_;
};

//-----------------------------------------------

template<class F>
auto
Response::
post(F&& f) -> route_result
{
    // task already posted
    if(task_)
        detail::throw_invalid_argument();

    struct BOOST_SYMBOL_VISIBLE
        immediate : detacher::owner
    {
        route_result rv;
        bool set = false;
        void do_resume(
            route_result const& rv_) override
        {
            rv = rv_;
            set = true;
        }
    };

    class BOOST_SYMBOL_VISIBLE model
        : public task
        , public detacher::owner
    {
    public:
        model(Response& res,
            F&& f, resumer resume)
            : res_(res)
            , f_(std::forward<F>(f))
            , resume_(resume)
        {
        }

        bool invoke() override
        {
            resumed_ = false;
            // VFALCO analyze exception safety
            f_(resumer(*this));
            return resumed_;
        }

        void do_resume(
            route_result const& rv) override
        {
            resumed_ = true;
            resumer resume(resume_);
            res_.task_.reset(); // destroys *this
            resume(rv);
        }

    private:
        Response& res_;
        typename std::decay<F>::type f_;
        resumer resume_;
        bool resumed_;
    };

    // first call
    immediate impl;
    f(resumer(impl));
    if(impl.set)
        return impl.rv;

    return detach(
        [&](resumer resume)
        {
            task_ = std::unique_ptr<task>(new model(
                *this, std::forward<F>(f), resume));
            do_post();
        });
}


} // beast2
} // boost

#endif
