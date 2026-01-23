//
// Copyright (c) 2019 Vinnie Falco (vinnie.falco@gmail.com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_HPP
#define BOOST_BEAST2_HPP

// Server components
#include <boost/beast2/server/any_lambda.hpp>
#include <boost/beast2/server/fixed_array.hpp>
#include <boost/beast2/http_server.hpp>
#include <boost/beast2/server/http_stream.hpp>
#include <boost/beast2/server/route_handler_corosio.hpp>
#include <boost/beast2/server/router.hpp>
#include <boost/beast2/server/router_corosio.hpp>
#include <boost/beast2/server/serve_static.hpp>

// Test utilities
#include <boost/beast2/test/error.hpp>
#include <boost/beast2/test/fail_count.hpp>

// Core utilities
#include <boost/beast2/buffer.hpp>
#include <boost/beast2/client.hpp>
#include <boost/beast2/endpoint.hpp>
#include <boost/beast2/error.hpp>
#include <boost/beast2/format.hpp>
#include <boost/beast2/log_service.hpp>
#include <boost/beast2/logger.hpp>

#endif
