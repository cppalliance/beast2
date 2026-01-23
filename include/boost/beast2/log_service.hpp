//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_LOG_SERVICE_HPP
#define BOOST_BEAST2_LOG_SERVICE_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/logger.hpp>
#include <boost/http/core/polystore.hpp>
#include <boost/core/detail/string_view.hpp>
#include <vector>

namespace boost {
namespace beast2 {

class BOOST_SYMBOL_VISIBLE
    log_service
{
public:
    /** Return a new or existing section by name
        If a section with the specified name does not exist,
        it is created.
        @param name The section name.
        @return The section.
    */
    virtual section get_section(
        core::string_view name) = 0;

    virtual
    auto
    get_sections() const noexcept ->
        std::vector<section> = 0;
};

/** Return the log service attached to the polystore
    If the polystore does not already contain the
    service, it is created.
    @param ps The polystore.
    @return The log service.
*/
BOOST_BEAST2_DECL
log_service&
use_log_service(
    http::polystore& ps);

} // beast2
} // boost

#endif
