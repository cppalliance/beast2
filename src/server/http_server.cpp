//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/server/http_server.hpp>
#include <boost/beast2/server/workers.hpp>
#include <boost/capy/application.hpp>
#include <boost/corosio/io_context.hpp>
#include <boost/corosio/endpoint.hpp>
#include <boost/url/ipv4_address.hpp>

namespace boost {
namespace beast2 {

namespace {

class http_server_impl
    : public http_server
{
public:
    http_server_impl(
        capy::application& app,
        char const* addr,
        unsigned short port,
        std::size_t num_workers)
        : ioc_()
        , workers_(app, ioc_, num_workers, this->wwwroot)
    {
        // Parse address and create endpoint
        auto addr_result = urls::parse_ipv4_address(addr);
        if (addr_result.has_error())
        {
            // Fallback to any address if parsing fails
            workers_.listen(
                http::acceptor_config{false, false},
                corosio::endpoint(port));
        }
        else
        {
            workers_.listen(
                http::acceptor_config{false, false},
                corosio::endpoint(addr_result.value(), port));
        }
    }

    void run() override
    {
        workers_.start();
        ioc_.run();
    }

    void stop() override
    {
        workers_.stop();
        ioc_.stop();
    }

private:
    corosio::io_context ioc_;
    workers workers_;
};

} // (anon)

//------------------------------------------------

http_server&
install_plain_http_server(
    capy::application& app,
    char const* addr,
    unsigned short port,
    std::size_t num_workers)
{
    return app.emplace<http_server_impl>(
        app, addr, port, num_workers);
}

} // beast2
} // boost
