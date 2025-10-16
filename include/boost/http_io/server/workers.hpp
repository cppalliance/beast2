//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_SERVER_WORKERS_HPP
#define BOOST_HTTP_IO_SERVER_WORKERS_HPP

#include <boost/http_io/detail/config.hpp>
#include <boost/http_io/server/fixed_array.hpp>
#include <boost/http_io/server/server.hpp>

namespace boost {
namespace http_io {

template<class Worker>
class workers : public server::part
{
public:
    template<class... Args>
    explicit
    workers(
        std::size_t n,
        Args const&... args)
    {
        while(! v_.is_full())
            v_.emplace(std::forward<Args>(args)...);
    }

    void run() override
    {
        for(auto w : v_)
            w.run();
    }

    void stop() override
    {
        for(auto w : v_)
            w.stop();
    }

private:
    fixed_array<Worker> v_;
};

} // http_io
} // boost

#endif
