//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_TEST_TEST_HELPERS_HPP
#define BOOST_BEAST2_TEST_TEST_HELPERS_HPP

#include <boost/asio/io_context.hpp>
#include <boost/core/exchange.hpp>
#include <boost/optional.hpp>

#include "test_suite.hpp"

namespace boost {
namespace beast2 {
namespace test {

/** A CompletionHandler used for testing.

    This completion handler is used by tests to ensure correctness
    of behavior. It is designed as a single type to reduce template
    instantiations, with configurable settings through constructor
    arguments. Typically this type will be used in type lists and
    not instantiated directly; instances of this class are returned
    by the helper functions listed below.

    @see success_handler, @ref fail_handler, @ref any_handler
*/
class handler
{
    boost::optional<system::error_code> ec_;
    bool pass_ = false;
    boost::source_location loc_{BOOST_CURRENT_LOCATION};

    public:
    handler(
        boost::source_location loc = BOOST_CURRENT_LOCATION)
        : loc_(loc)
    {
    }

    explicit
    handler(
        system::error_code ec,
        boost::source_location loc = BOOST_CURRENT_LOCATION)
        : ec_(ec)
        , loc_(loc)
    {
    }

    explicit
    handler(
        boost::none_t,
        boost::source_location loc = BOOST_CURRENT_LOCATION)
        : loc_(loc)
    {
    }

    handler(handler&& other)
        : ec_(other.ec_)
        , pass_(boost::exchange(other.pass_, true))
        , loc_(other.loc_)

    {
    }

    ~handler()
    {
        test_suite::any_runner::instance().test(
            pass_,
            "handler never invoked",
            "",
            loc_.file_name(),
            loc_.line());
    }

    template<class... Args>
    void
    operator()(system::error_code ec, Args&&...)
    {
        test_suite::any_runner::instance().test(
            !pass_,
            "handler invoked multiple times",
            "",
            loc_.file_name(),
            loc_.line());

        test_suite::any_runner::instance().test(
            !ec_.has_value() || ec == *ec_,
            ec.message().c_str(),
            "",
            loc_.file_name(),
            loc_.line());
        pass_ = true;
    }
};

/** Return a test CompletionHandler which requires success.
    
    The returned handler can be invoked with any signature whose
    first parameter is an `system::error_code`. The handler fails the test
    if:

    @li The handler is destroyed without being invoked, or

    @li The handler is invoked with a non-successful error code.
*/
inline
handler
success_handler(boost::source_location loc = BOOST_CURRENT_LOCATION) noexcept
{
    return handler(system::error_code{}, loc);
}

/** Return a test CompletionHandler which requires invocation.

    The returned handler can be invoked with any signature.
    The handler fails the test if:

    @li The handler is destroyed without being invoked.
*/
inline
handler
any_handler(boost::source_location loc = BOOST_CURRENT_LOCATION) noexcept
{
    return handler(boost::none, loc);
}

/** Return a test CompletionHandler which requires a specific error code.

    This handler can be invoked with any signature whose first
    parameter is an `system::error_code`. The handler fails the test if:

    @li The handler is destroyed without being invoked.

    @li The handler is invoked with an error code different from
    what is specified.

    @param ec The error code to specify.
*/
inline
handler
fail_handler(system::error_code ec,boost::source_location loc = BOOST_CURRENT_LOCATION) noexcept
{
    return handler(ec, loc);
}

/** Run an I/O context.
    
    This function runs and dispatches handlers on the specified
    I/O context, until one of the following conditions is true:
        
    @li The I/O context runs out of work.

    @param ioc The I/O context to run
*/
inline
std::size_t
run(asio::io_context& ioc)
{
    std::size_t n = ioc.run();
    ioc.restart();
    return n;
}

/** Run an I/O context for a certain amount of time.
    
    This function runs and dispatches handlers on the specified
    I/O context, until one of the following conditions is true:
        
    @li The I/O context runs out of work.

    @li No completions occur and the specified amount of time has elapsed.

    @param ioc The I/O context to run

    @param elapsed The maximum amount of time to run for.
*/
template<class Rep, class Period>
std::size_t
run_for(
    asio::io_context& ioc,
    std::chrono::duration<Rep, Period> elapsed)
{
    std::size_t n = ioc.run_for(elapsed);
    ioc.restart();
    return n;
}

} // test
} // beast2
} // boost

#endif
