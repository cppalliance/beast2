//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_EXAMPLE_SERVER_HANDLER_HPP
#define BOOST_BEAST2_EXAMPLE_SERVER_HANDLER_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/server/route_params.hpp>
#include <boost/beast2/server/router.hpp>
#include <boost/http_proto/request_base.hpp>
#include <boost/http_proto/response.hpp>
#include <boost/http_proto/serializer.hpp>
#include <boost/core/detail/string_view.hpp>

namespace boost {
namespace beast2 {

using router_type = router<Response>;

//------------------------------------------------

struct https_redirect_responder
{
    bool operator()(Request&, Response&) const;
};

//------------------------------------------------

struct file_responder
{
    file_responder(
        core::string_view doc_root)
        : doc_root_(doc_root)
    {
    }

    bool operator()(Request&, Response&) const;

private:
    std::string doc_root_;
};

} // beast2
} // boost

#endif
