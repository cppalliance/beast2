//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_SERVER_ASIO_HPP
#define BOOST_BEAST2_SERVER_SERVER_ASIO_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/server/server.hpp>
#include <boost/asio/io_context.hpp>

namespace boost {
namespace beast2 {

/** A server using Boost.Asio's I/O model
*/
class BOOST_SYMBOL_VISIBLE server_asio
    : public server
{
public:
    using executor_type =
        asio::io_context::executor_type;

    /** Destructor
    */
    BOOST_BEAST2_DECL
    ~server_asio();

    /** Constructor
    */
    BOOST_BEAST2_DECL
    server_asio(int num_threads);

    BOOST_BEAST2_DECL
    executor_type
    get_executor() noexcept;

    /** Run the server

        This function blocks until the server is stopped.
    */
    BOOST_BEAST2_DECL
    void run();

    /** Stop the server
    */
    BOOST_BEAST2_DECL
    void stop() override;

private:
    struct impl;

    void on_signal(system::error_code const&, int);
    void on_timer(system::error_code const&);

    impl* impl_;
};

} // beast2
} // boost

#endif
