//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/http_io
//

#include "test_suite.hpp"

#if 0

#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/http_proto.hpp>
#include <boost/core/detail/string_view.hpp>
#include <chrono>
#include <memory>
#include <sstream>

namespace boost {
namespace http_proto {

static std::size_t constexpr buffer_bytes = 8 * 1024 * 1024;
static std::size_t constexpr payload_size = buffer_bytes * 10;

using tcp = asio::ip::tcp;
namespace ssl = asio::ssl;
using string_view =
    boost::core::string_view;

//static auto const& log = test_suite::log;
static std::stringstream log;

struct logging_socket : asio::ip::tcp::socket
{
  using asio::ip::tcp::socket::socket;

  template <typename ConstBufferSequence,
      BOOST_ASIO_COMPLETION_TOKEN_FOR(void (boost::system::error_code,
                                          std::size_t)) WriteToken
      BOOST_ASIO_DEFAULT_COMPLETION_TOKEN_TYPE(executor_type)>
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE_PREFIX(WriteToken,
                                            void (boost::system::error_code, std::size_t))
  async_write_some(const ConstBufferSequence& buffers,
                   BOOST_ASIO_MOVE_ARG(WriteToken) token
                   BOOST_ASIO_DEFAULT_COMPLETION_TOKEN(executor_type))
  BOOST_ASIO_INITFN_AUTO_RESULT_TYPE_SUFFIX((
                                                async_initiate<WriteToken,
                                                    void (boost::system::error_code, std::size_t)>(
                                                    declval<initiate_async_send>(), token,
                                                    buffers, socket_base::message_flags(0))))
  {
    return asio::ip::tcp::socket::async_write_some(
        buffers,
        asio::deferred([](boost::system::error_code ec, std::size_t n)
                       {
                          log << "async_write_some: " << n << "\n";
                          return asio::deferred.values(ec, n);
                       }))(std::forward<WriteToken>(token));
  }

};

class my_transfer_all_t
{
public:
  typedef std::size_t result_type;

  template <typename Error>
  std::size_t operator()(const Error& err, std::size_t)
  {
    return !!err ? 0 : std::size_t(-1);
  }
};

static constexpr my_transfer_all_t my_transfer_all{};

struct sandbox_test
{
    void
    do_write(
        asio::io_context& ioc,
        asio::yield_context yield)
    {
        using clock_type =
            std::chrono::high_resolution_clock;

        tcp::resolver dns(ioc);
        logging_socket sock(ioc);
        std::unique_ptr<char[]> up(new char[buffer_bytes]);
        asio::mutable_buffer mb(up.get(), buffer_bytes);
        request req;

        asio::async_connect(
            sock,
            dns.resolve(
                "httpbin.cpp.al", "http"),
            yield);

        // header
        req.set_start_line(
            method::post, "/post", version::http_1_1);
        req.append(field::host, "httpbin.cpp.al");
        req.append(field::accept, "application/text");
        req.append(field::user_agent, "boost");
        req.set_payload_size(payload_size);
        asio::async_write(
            sock,
            asio::buffer(req.buffer()),
            yield);

        // body
        std::size_t n = 0;
        std::size_t count = 0;
        while(n < payload_size)
        {
            auto amount = payload_size - n;
            if( amount > buffer_bytes)
                amount = buffer_bytes;
            auto const t0 = clock_type::now();
            auto bytes_transferred =
                asio::async_write(
                    sock,
                    asio::mutable_buffer(
                        mb.data(),
                        amount),
                    my_transfer_all,
                    yield);
            auto const ms = std::chrono::duration_cast<
                std::chrono::milliseconds>(
                    clock_type::now() - t0).count();
            log <<
                "write " << bytes_transferred <<
                " bytes in " << ms <<
                "ms\n";

            n += bytes_transferred;
            ++count;
        }
        log << count << " writes total\n\n";
    }

    void
    do_write_some(
        asio::io_context& ioc,
        asio::yield_context yield)
    {
        using clock_type =
            std::chrono::high_resolution_clock;
        
        tcp::resolver dns(ioc);
        tcp::socket sock(ioc);
        std::unique_ptr<char[]> up(new char[buffer_bytes]);
        asio::mutable_buffer mb(up.get(), buffer_bytes);
        request req;

        asio::async_connect(
            sock,
            dns.resolve(
                "httpbin.cpp.al", "http"),
            yield);

        // header
        req.set_start_line(
            method::post, "/post", version::http_1_1);
        req.append(field::host, "httpbin.cpp.al");
        req.append(field::accept, "application/text");
        req.append(field::user_agent, "boost");
        req.set_payload_size(payload_size);
        asio::async_write(
            sock,
            asio::buffer(req.buffer()),
            yield);

        // body
        std::size_t n = 0;
        std::size_t count = 0;
        while(n < payload_size)
        {
            auto amount = payload_size - n;
            if( amount > buffer_bytes)
                amount = buffer_bytes;
            auto const t0 = clock_type::now();
            auto bytes_transferred =
                sock.async_write_some(
                    asio::mutable_buffer(
                        mb.data(),
                        amount),
                    yield);
            auto const ms = std::chrono::duration_cast<
                std::chrono::milliseconds>(
                    clock_type::now() - t0).count();
            log <<
                "write_some " << bytes_transferred <<
                " bytes in " << ms <<
                "ms\n";

            n += bytes_transferred;
            ++count;
        }
        log << count << " write_somes total\n\n";
    }

    void
    run()
    {
        asio::io_context ioc;
        asio::spawn(ioc, [&](
            asio::yield_context yield)
            {
                do_write( ioc, yield );
                do_write_some( ioc, yield );
            });
        ioc.run();
        test_suite::log << log.str();
    }
};

TEST_SUITE(
    sandbox_test,
    "boost.http_io.sandbox");

} // http_proto
} // boost

#endif
