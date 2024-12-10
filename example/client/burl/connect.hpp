//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BURL_CONNECT_HPP
#define BURL_CONNECT_HPP

#include "any_stream.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/http_proto/context.hpp>
#include <boost/program_options.hpp>
#include <boost/url/url.hpp>

namespace asio       = boost::asio;
namespace http_proto = boost::http_proto;
namespace po         = boost::program_options;
namespace ssl        = boost::asio::ssl;
namespace urls       = boost::urls;

asio::awaitable<boost::optional<any_stream>>
connect(
    const po::variables_map& vm,
    ssl::context& ssl_ctx,
    http_proto::context& http_proto_ctx,
    urls::url url);

#endif
