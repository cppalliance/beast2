//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include "async_42.hpp"
#include <boost/beast2/spawn.hpp>
#include <boost/buffers/any_stream.hpp>
#include <boost/buffers/slice.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <iostream>
#include <variant>

#include <boost/buffers/buffer.hpp>

namespace boost {

namespace buffers {

auto read_all(
    any_stream s,
    buffers::mutable_buffer b)
        -> capy::task<io_result>
{
    std::size_t total = 0;
    while(b.size() > 0)
    {
        auto [ec, n] = co_await s.read_some(b);
        total += n;
        if(ec.failed())
            co_return { ec, total };
        buffers::remove_prefix(b, n);
    }
    co_return { {}, total };
}

} // buffers

namespace beast2 {

struct stream
{
    asio::any_io_executor ex_;

    template<class Executor>
    stream(Executor const& ex)
        : ex_(ex)
    {
    }

    capy::async_result<int>
    read_some()
    {
        return capy::make_async_result<int>(
            async_42(ex_, asio::deferred));
    }
};

} // beast2

capy::task<int> handler()
{
    co_return 42;
}

void boost_main()
{
    asio::io_context ioc;

    beast2::spawn(
        ioc.get_executor(),
        handler(),
        [](std::variant<std::exception_ptr, int> result)
        {
            if (result.index() == 0)
                std::rethrow_exception(std::get<0>(result));
            std::cout << "result: " << std::get<1>(result) << "\n";
        });

    ioc.run();
}

} // boost

int main(int, char**)
{
    boost::boost_main();
}
