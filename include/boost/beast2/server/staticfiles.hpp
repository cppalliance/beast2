//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_STATICFILES_HPP
#define BOOST_BEAST2_SERVER_STATICFILES_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/server/route_params.hpp>
#include <memory>

namespace boost {
namespace beast2 {

/** A route handler which serves static files from a document root
*/
struct staticfiles
{
    BOOST_BEAST2_DECL
    ~staticfiles();

    /** Constructor
        @param path The document root path
    */
    BOOST_BEAST2_DECL
    explicit
    staticfiles(
        core::string_view path);

    BOOST_BEAST2_DECL
    staticfiles(staticfiles&&) noexcept;

    BOOST_BEAST2_DECL
    bool operator()(Request&, Response&) const;

private:
    struct impl;
    std::unique_ptr<impl> impl_;
};

} // beast2
} // boost

#endif
