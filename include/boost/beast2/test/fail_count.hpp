//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppaliance/beast2
//

#ifndef BOOST_BEAST2_TEST_FAIL_COUNT_HPP
#define BOOST_BEAST2_TEST_FAIL_COUNT_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/test/error.hpp>

#include <boost/system/system_error.hpp>
#include <boost/throw_exception.hpp>

#include <cstdlib>

namespace boost {
namespace beast2 {
namespace test {

/** A countdown to simulated failure

    On the Nth operation, the class will fail with the specified
    error code, or the default error code of @ref error::test_failure.

    Instances of this class may be used to build objects which
    are specifically designed to aid in writing unit tests, for
    interfaces which can throw exceptions or return `error_code`
    values representing failure.
*/
class fail_count
{
    std::size_t n_;
    std::size_t i_ = 0;
    system::error_code ec_;

public:
    fail_count(fail_count&&) = default;

    /** Construct a counter

        @param n The 0-based index of the operation to fail on or after
        @param ev An optional error code to use when generating a simulated failure
    */
    explicit
    fail_count(
        std::size_t n,
        system::error_code ev = error::test_failure)
        : n_(n)
        , ec_(ev)
    {
    }

    /// Throw an exception on the Nth failure
    void
    fail()
    {
        if(i_ < n_)
            ++i_;
        if(i_ == n_)
            BOOST_THROW_EXCEPTION(system::system_error{ec_});
    }

    /// Set an error code on the Nth failure
    bool
    fail(system::error_code& ec)
    {
        if(i_ < n_)
            ++i_;
        if(i_ == n_)
        {
            ec = ec_;
            return true;
        }
        ec = {};
        return false;
    }
};

} // test
} // beast2
} // boost

#endif
