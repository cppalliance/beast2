//
// Copyright (c) 2025 Mungo Gill
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/body_read_stream.hpp>

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/read.hpp>
#include <boost/buffers/make_buffer.hpp>
#include <boost/capy/polystore.hpp>

#include "test_helpers.hpp"
#include <boost/beast2/test/stream.hpp>

#include <algorithm>
#include <iostream>

namespace boost {
namespace beast2 {

template<class Buffers>
std::string
test_to_string(Buffers const& bs)
{
    std::string s(buffers::size(bs), 0);
    s.resize(buffers::copy(buffers::make_buffer(&s[0], s.size()), bs));
    return s;
}

class test_handler
{
    boost::optional<system::error_code> ec_;
    boost::optional<std::size_t> n_;
    bool pass_ = false;
    boost::source_location loc_{ BOOST_CURRENT_LOCATION };

public:
    test_handler(boost::source_location loc = BOOST_CURRENT_LOCATION)
        : loc_(loc)
    {
    }

    explicit test_handler(
        system::error_code ec,
        std::size_t n,
        boost::source_location loc = BOOST_CURRENT_LOCATION)
        : ec_(ec)
        , n_(n)
        , loc_(loc)
    {
    }

    test_handler(test_handler&& other)
        : ec_(other.ec_)
        , n_(other.n_)
        , pass_(boost::exchange(other.pass_, true))
        , loc_(other.loc_)
    {
    }

    ~test_handler()
    {
        test_suite::any_runner::instance().test(
            pass_, "handler never invoked", "", loc_.file_name(), loc_.line());
    }

    template<class... Args>
    void
    operator()(system::error_code ec, std::size_t n, Args&&...)
    {
        test_suite::any_runner::instance().test(
            !pass_,
            "handler invoked multiple times",
            "",
            loc_.file_name(),
            loc_.line());

        test_suite::any_runner::instance().test(
            !ec_.has_value() || ec == *ec_,
            ec.message().c_str(),
            "",
            loc_.file_name(),
            loc_.line());

        char buf[64];
        snprintf(buf, 64, "%u", (unsigned int) n);

        test_suite::any_runner::instance().test(
            !n_.has_value() || n == *n_,
            buf,
            "",
            loc_.file_name(),
            loc_.line());

        pass_ = true;
    }
};

// Parser service install done in a base class to avoid order-of-initialisation
// issues (this needs to happen before the parser pr_ is constructed)
struct ctx_base
{
    capy::polystore capy_ctx_;
    http_proto::prepared_parser_config cfg_;

    ctx_base()
        : cfg_(http_proto::parser_config(
            http_proto::role::client,
                capy_ctx_).prepare())
    {
    }
};

struct single_tester : public ctx_base
{
    std::string body_ = "Hello World!";

    std::string header_ =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 12\r\n"
        "\r\n";

    std::string msg_ = header_ + body_;

    std::size_t header_length_ = header_.size();
    std::size_t body_length_ = body_.size();
    std::size_t msg_length_ = msg_.size();

    boost::asio::io_context ioc_;

    test::stream ts_;
    http_proto::response_parser pr_;

    // Create a destination buffer
    std::string s_;
    boost::buffers::string_buffer buf_;

    // The object under test
    body_read_stream<test::stream> brs_;

    single_tester()
        : ts_(ioc_, msg_)
        , pr_(cfg_)
        , buf_(&s_)
        , brs_(ts_, pr_)
    {
        pr_.reset();
        pr_.start();
    }

    void
    async_read_some(std::size_t bs, system::error_code ec, std::size_t n)
    {
        brs_.async_read_some(
            buf_.prepare(bs),
            test_handler(ec, n));
    }


    std::size_t
    chunking_expected_n(
        std::size_t bs,
        std::size_t cs,
        bool first,
        std::size_t read_so_far)
    {
        std::size_t expected = 0;
        if(read_so_far < body_length_)
        {
            expected = cs;
            // In the first iteration we remove any of the data that was
            // associcated with the headers.
            if(first)
            {
                expected -= (header_length_ % cs);
                // The `beast2::async_read_some` will always read move from
                // the wire immediately after the headers, even if we have a
                // partial body in memory already. This should be removable
                // once `async_read_some` changes.
                if(expected < cs)
                {
                    expected += cs;
                }
            }
            expected = std::min(expected, body_length_ - read_so_far);
            expected = std::min(bs, expected);
        }
        return expected;
    }

    struct chunking_handler
    {
        std::size_t n_;
        system::error_code ec_;
        std::size_t* total_;
        boost::buffers::string_buffer* buf_;

        chunking_handler(
            std::size_t n,
            system::error_code ec,
            std::size_t* total,
            boost::buffers::string_buffer* buf)
            : n_(n)
            , ec_(ec)
            , total_(total)
            , buf_(buf)
        {
        }

        void
        operator()(system::error_code ec, std::size_t n)
        {
            BOOST_TEST_EQ(ec, ec_);
            BOOST_TEST_EQ(n, n_);
            buf_->commit(n);
            *total_ += n;
        }
    };

    
    // Ensure the edge case of being passed a zero-sized buffer works.
    void
    test_zero_sized_buffer()
    {
        // Ensure a read into a zero sized buffer returns with no error.
        async_read_some(0, system::error_code{}, 0);
        test::run(ioc_);
    }

    // Test for a given buffer size (bs) and stream read size (cs)
    void
    test_with_chunking(std::size_t bs, std::size_t cs)
    {
        ts_.read_size(cs); // Limit read size to cs

        std::size_t total = 0;
        for(std::size_t i = 0; i < body_length_; i++)
        {
            // Calculate how many bytes we expect to read on each iteration
            std::size_t expected = chunking_expected_n(bs, cs, (i == 0), total);

            // Read into a buffer of size bs
            brs_.async_read_some(
                buf_.prepare(bs),
                chunking_handler(
                    expected,
                    (total < body_length_) ? system::error_code{}
                                           : asio::error::eof,
                    &total,
                    &buf_));

            auto count = test::run(ioc_);
            if(i > 0) // The initial run reads the header so can call multiple handlers.
                BOOST_TEST_EQ(count, 1);

            BOOST_TEST(pr_.got_header());
        }

        BOOST_TEST(pr_.is_complete());
        BOOST_TEST_EQ(buf_.size(), body_length_);
        BOOST_TEST_EQ(total, body_length_);
        BOOST_TEST(test_to_string(buf_.data()) == body_);
    }

    void
    test_with_cancellation(std::size_t len)
    {
        ts_.clear();
        ts_.append(msg_.substr(0, len));

        // Add a signal to test cancellation
        asio::cancellation_signal c_signal;

        brs_.async_read_some(
            buf_.prepare(1024),
            asio::bind_cancellation_slot(
                c_signal.slot(),
                test_handler(asio::error::operation_aborted, 0)));

        // send a cancellation
        c_signal.emit(asio::cancellation_type::total);

        // Run up until the point of cancellation.
        test::run(ioc_);

        BOOST_TEST(pr_.got_header() == (len >= header_length_));
        BOOST_TEST(!pr_.is_complete());

        // Append the remainder of the message and try again.
        std::string remainder = msg_.substr(len);
        ts_.append(remainder);
        brs_.async_read_some(
            buf_.prepare(1024),
            test_handler(system::error_code{}, body_length_));

        // Continue running until the end.
        test::run(ioc_);

        BOOST_TEST(pr_.got_header());
        BOOST_TEST(pr_.is_complete());
    }

    void
    test_asio_async_read(std::size_t cs, bool use_asio_buffer)
    {
        // limit chunk size
        ts_.read_size(cs);

        if (use_asio_buffer)
        {
            asio::async_read(
                brs_,
                asio::buffer(buf_.prepare(1024).data(), 1024),
                test_handler(asio::error::eof, body_length_));
        } else {
            asio::async_read(
                brs_,
                buf_.prepare(1024),
                test_handler(asio::error::eof, body_length_));
        }

        test::run(ioc_);

        buf_.commit(body_length_);

        BOOST_TEST(pr_.got_header());
        BOOST_TEST(pr_.is_complete());

        BOOST_TEST_EQ(buf_.size(), body_length_);
        BOOST_TEST(test_to_string(buf_.data()) == body_);
    }

    void
    test_stream_errors()
    {
        // Replace the test stream by one with a failure count and a single-byte
        // read size.
        test::fail_count fc(11, asio::error::network_down);
        test::stream ts(ioc_, fc, msg_);
        ts.read_size(1);
        ts_ = std::move(ts);

        async_read_some(1024, asio::error::network_down, 0);

        BOOST_TEST_EQ(test::run(ioc_), 11);
    }

    void
    test_parser_errors()
    {
        // Ensure we get an error by making the body limit too small
        pr_.set_body_limit(2);

        async_read_some(1024, http_proto::error::body_too_large, 0);

        test::run(ioc_);
    }
};

struct body_read_stream_test
{
    void
    run()
    {
        std::size_t msg_length = single_tester().msg_length_;

        // Read into a zero sized buffer should return immediately without error
        {
            single_tester().test_zero_sized_buffer();
        }

        // async_read_some reads the body for various chunk
        // sizes.
        {
            // Iterate through buffer sizes
            for(std::size_t bs = 1; bs < msg_length + 2; bs++)
            {
                // Iterate through chunk sizes
                for(std::size_t cs = 1; cs < msg_length + 2; cs++)
                {
                    single_tester().test_with_chunking(bs, cs);
                }
            }
        }

        // Test async_read_some cancellation
        {
            // Iterate through the point in the message the cancellation happens.
            for(std::size_t len = 1; len < msg_length; len++)
            {
                single_tester().test_with_cancellation(len);
            }
        }

        // Test asio::async_read works
        {
            // pick a representative chunk, as we already do the looping over
            // all chunks above.
            std::size_t cs = 5;

            // Perform the test using the Boost Buffers buffer directly
            single_tester().test_asio_async_read(cs, false);
            // And again using an asio buffer wrapper
            single_tester().test_asio_async_read(cs, true);
        }

        // async_read_some reports stream errors
        {
            single_tester().test_stream_errors();
        }

        // async_read_some reports parser errors
        {
            single_tester().test_parser_errors();

        }
    }
};

TEST_SUITE(body_read_stream_test, "boost.beast2.body_read_stream");

} // beast2
} // boost
