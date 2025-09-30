//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_EXAMPLE_ASIO_SERVER_HPP
#define BOOST_HTTP_IO_EXAMPLE_ASIO_SERVER_HPP

#include "server.hpp"
#include <boost/asio/basic_waitable_timer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

/** A server using Boost.Asio's I/O model
*/
class asio_server : public server
{
public:
    using executor_type =
        boost::asio::io_context::executor_type;

    executor_type
    get_executor() noexcept
    {
        return ioc_.get_executor();
    }

    asio_server();
    void run();
    void stop();

private:
    void on_signal(boost::system::error_code const&, int);
    void on_timer(boost::system::error_code const&);

    boost::asio::io_context ioc_;
    boost::asio::signal_set sigs_;
    boost::asio::basic_waitable_timer<
        std::chrono::steady_clock,
        boost::asio::wait_traits<std::chrono::steady_clock>,
        executor_type> timer_;
};

#endif
