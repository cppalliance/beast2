//
// Copyright (c) 2025 Amlal El Mahrouss (amlal at nekernel dot org)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_SERVE_REDIRECT_HPP
#define BOOST_BEAST2_SERVER_SERVE_REDIRECT_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/http_proto/server/route_handler.hpp>

namespace boost {
namespace beast2 {

struct serve_not_found
{
private:
    struct serve_impl;
    serve_impl* impl_{};

public:
    serve_not_found(const core::string_view&);
    ~serve_not_found();

    serve_not_found& operator=(serve_not_found&&) noexcept;
    serve_not_found(serve_not_found&&) noexcept;

    /** Serves a 404.html when a resource isn't found. 
        @note if no 404.html is found the route will be ignored.
    */
    BOOST_BEAST2_DECL
    http::route_result
    operator()(
        http::route_params&) const;
};

} // beast2
} // boost

#endif
