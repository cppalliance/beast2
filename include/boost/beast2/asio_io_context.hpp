//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_ASIO_IO_CONTEXT_HPP
#define BOOST_BEAST2_ASIO_IO_CONTEXT_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/capy/application.hpp>
#include <boost/asio/io_context.hpp>

namespace boost {
namespace beast2 {

/** Asio's io_context as an application part
*/
class BOOST_SYMBOL_VISIBLE
    asio_io_context
{
public:
    using executor_type =
        asio::io_context::executor_type;

    /** Destructor
    */
    BOOST_BEAST2_DECL
    ~asio_io_context();

    virtual
    executor_type
    get_executor() noexcept = 0;

    /** Return the concurrency level
    */
    virtual
    std::size_t
    concurrency() const noexcept = 0;

    /** Run the context

        This function attaches the current thread to I/O context
        so that it may be used for executing submitted function
        objects. Blocks the calling thread until the part is stopped
        and has no outstanding work.
    */
    virtual void attach() = 0;

    virtual void start() = 0;
    virtual void stop() = 0;
};

BOOST_BEAST2_DECL
auto
install_single_threaded_asio_io_context(
    capy::application& app) ->
        asio_io_context&;

BOOST_BEAST2_DECL
auto
install_multi_threaded_asio_io_context(
    capy::application& app,
    int num_threads) ->
        asio_io_context&;

} // beast2
} // boost

#endif
