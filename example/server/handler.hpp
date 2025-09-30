//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_EXAMPLE_SERVER_HANDLER_HPP
#define BOOST_HTTP_IO_EXAMPLE_SERVER_HANDLER_HPP

#include <boost/http_proto/request_base.hpp>
#include <boost/http_proto/response.hpp>
#include <boost/http_proto/serializer.hpp>
#include <boost/core/detail/string_view.hpp>

void
handle_request(
    boost::core::string_view doc_root,
    boost::http_proto::request_base const& req,
    boost::http_proto::response& res,
    boost::http_proto::serializer& sr);

#endif
