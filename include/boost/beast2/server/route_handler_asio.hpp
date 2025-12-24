//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_ROUTE_HANDLER_ASIO_HPP
#define BOOST_BEAST2_SERVER_ROUTE_HANDLER_ASIO_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/spawn.hpp>
#include <boost/http_proto/server/route_handler.hpp>
#include <boost/asio/post.hpp>
#include <type_traits>

namespace boost {
namespace beast2 {

/** Route parameters object for Asio HTTP route handlers
*/
template<class AsyncStream>
class asio_route_params
    : public http::route_params
{
public:
    using stream_type = typename std::decay<AsyncStream>::type;

    AsyncStream stream;

    template<class... Args>
    explicit
    asio_route_params(
        Args&&... args)
        : stream(std::forward<Args>(args)...)
    {
    }

private:
#ifdef BOOST_CAPY_HAS_CORO
    auto
    spawn(
        capy::task<http::route_result> t) ->
            http::route_result override
    {
        return this->suspend(
            [&](http::resumer resume)
            {
                beast2::spawn(
                    stream.get_executor(),
                    std::move(t),
                    [resume](std::variant<
                        std::exception_ptr,
                        http::route_result> v)
                    {
                        if(v.index() == 0)
                        {
                            std::rethrow_exception(
                                std::get<0>(v));
                        }
                        resume(std::get<1>(v));
                    });
            });
    }
#endif

    void do_post() override;
};

//-----------------------------------------------

template<class AsyncStream>
void
asio_route_params<AsyncStream>::
do_post()
{
    asio::post(
        stream.get_executor(),
        [&]
        {
            if(task_->invoke())
                return;
            do_post();
        });
}

} // beast2
} // boost

#endif
