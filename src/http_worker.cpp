//
// Copyright (c) 2026 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/http_worker.hpp>
#include <boost/http/error.hpp>
#include <boost/url/parse.hpp>
#include <iostream>

namespace boost {
namespace beast2 {

http_worker::
http_worker(
    http::flat_router fr_,
    http::shared_parser_config parser_cfg,
    http::shared_serializer_config serializer_cfg)
    : fr(std::move(fr_))
    , parser(parser_cfg)
    , serializer(serializer_cfg)
{
    serializer.set_message(rp.res);
}

capy::task<void>
http_worker::
do_http_session()
{
    struct guard
    {
        http_worker& self;

        guard(http_worker& self_)
            : self(self_)
        {
        }

        ~guard()
        {
            self.parser.reset();
            self.parser.start();
            self.rp.session_data.clear();
        }
    };

    guard g(*this); // clear things when session ends

    // read request, send response loop
    for(;;)
    {
        parser.reset();
        parser.start();
        rp.session_data.clear();

        // Read HTTP request header
        auto [ec] = co_await parser.read_header(stream);
        if(ec)
        {
            std::cerr << "read_header error: " << ec.message() << "\n";
            break;
        }

        // Process headers and dispatch
        // Set up Request and Response objects
        rp.req = parser.get();
        rp.route_data.clear();
        rp.res.set_start_line(
            http::status::ok, rp.req.version());
        rp.res.set_keep_alive(rp.req.keep_alive());
        serializer.reset();

        // Parse the URL
        {
            auto rv = urls::parse_uri_reference(rp.req.target());
            if(rv.has_error())
            {
                rp.status(http::status::bad_request);
            }
            rp.url = rv.value();
        }

        {
            auto rv = co_await fr.dispatch(rp.req.method(), rp.url, rp);
            if(rv.failed())
            {
                // VFALCO log rv.error()
                break;
            }

            if(! rp.res.keep_alive())
                break;
        }
    }
}

} // beast2
} // boost
