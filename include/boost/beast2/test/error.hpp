//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppaliance/beast2
//

#ifndef BOOST_BEAST2_TEST_ERROR_HPP
#define BOOST_BEAST2_TEST_ERROR_HPP

namespace boost {
namespace beast2 {
namespace test {

/// Error codes returned from unit testing algorithms
enum class error
{
    /** The test stream generated a simulated testing error

        This error is returned by a @ref fail_count object
        when it generates a simulated error.
    */
    test_failure = 1
};

} // test
} // beast2
} // boost

#include <boost/beast2/test/impl/error.hpp>

#endif
