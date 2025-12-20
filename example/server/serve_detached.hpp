//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_SERVE_DETACHED_HPP
#define BOOST_BEAST2_SERVER_SERVE_DETACHED_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/error.hpp>
#include <boost/http_proto/server/route_handler.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <memory>
#include <thread>

namespace boost {
namespace beast2 {

/** A route handler which delegates work to a foreign thread
*/
class serve_detached
{
public:
    ~serve_detached()
    {
    }

    serve_detached()
        : tp_(new asio::thread_pool(1))
    {
    }

    serve_detached(serve_detached&&) = default;

    system::error_code
    operator()(
        http_proto::route_params& rp) const
    {
        return rp.suspend(
            [&](http_proto::resumer resume)
            {
                asio::post(*tp_,
                    [&, resume]()
                    {
                        // Simulate some asynchronous work
                        std::this_thread::sleep_for(std::chrono::seconds(1));
                        rp.status(http_proto::status::ok);
                        rp.set_body("Hello from serve_detached!\n");
                        resume(http_proto::route::send);
                        // resume( res.send("Hello from serve_detached!\n") );
                    });
            });
    }

private:
    std::unique_ptr<asio::thread_pool> tp_;
};

} // beast2
} // boost

#endif
