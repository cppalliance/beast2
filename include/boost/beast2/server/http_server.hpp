//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_HTTP_SERVER_HPP
#define BOOST_BEAST2_SERVER_HTTP_SERVER_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/server/router_asio.hpp>
#include <boost/capy/application.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <string>

namespace boost {
namespace beast2 {

template<class AsyncStream>
class http_server
{
public:
    ~http_server() = default;

    http_server() = default;

    router_asio<AsyncStream&> wwwroot;

    /** Run the server

        This function attaches the current thread to I/O context
        so that it may be used for executing submitted function
        objects. Blocks the calling thread until the part is stopped
        and has no outstanding work.
    */
    virtual void attach() = 0;
};

//------------------------------------------------

/**
    @brief HTTP Configuration structure for 'install_plain_http_server'.
*/
struct http_config final {
    ~http_config() = default;

    http_config() = default;

    std::size_t http_num_workers_{0UL};
    std::string http_addr_{};
    std::uint16_t http_port_{0U};

    static http_config make_config(const char** argv)
    {
        http_config config{};
     
        if (!argv || *argv == nullptr) return config;
     
        config.http_num_workers_ = (unsigned short)std::atoi(argv[2]);
        config.http_addr_ = argv[1];
        config.http_port_ = std::atoi(argv[4]);

        return config;
    }
};

//------------------------------------------------

BOOST_BEAST2_DECL
auto
install_plain_http_server(
    capy::application& app,
    http_config& config) ->
        http_server<asio::basic_stream_socket<
            asio::ip::tcp,
            asio::io_context::executor_type>>&;

} // beast2
} // boost

#endif
