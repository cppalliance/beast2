//
// Copyright (c) 2025 Mungo Gill
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/body_read_stream.hpp>
#include <boost/beast2/body_write_stream.hpp>
#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/wrap_executor.hpp>

#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/deferred.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/write.hpp>
#include <boost/capy/buffers/make_buffer.hpp>
#include <boost/capy/async_op.hpp>
#include <boost/capy/async_run.hpp>
#include <boost/capy/polystore.hpp>
#include <boost/capy/task.hpp>
#include <boost/http/response.hpp>
#include <boost/http/request.hpp>

#include "test_helpers.hpp"
#include <boost/beast2/test/stream.hpp>

#include <algorithm>
#include <iostream>
#include <optional>

namespace boost {
namespace beast2 {

namespace {

template<class Buffers>
std::string
test_to_string(Buffers const& bs)
{
    std::string s(capy::buffer_size(bs), 0);
    s.resize(capy::copy(capy::make_buffer(&s[0], s.size()), bs));
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
        std::size_t n = 0,
        boost::source_location loc = BOOST_CURRENT_LOCATION)
        : ec_(ec)
        , n_(n)
        , loc_(loc)
    {
    }

    test_handler(test_handler&& other) noexcept
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
    operator()(system::error_code ec, std::size_t n = 0, Args&&...)
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
        snprintf(buf, 64, "%u", (unsigned int)n);

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

    ctx_base()
    {
        http::install_parser_service(capy_ctx_, {});
        http::install_serializer_service(capy_ctx_, {});
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
    http::response_parser pr_;

    // Create a destination buffer
    std::string s_;
    boost::capy::string_buffer buf_;

    // The object under test
    body_read_stream<test::stream> brs_;

    test::stream wts_, rts_;

    http::serializer sr_;

    http::response res_;

    std::size_t srs_capacity_;

    std::optional<body_write_stream<test::stream>> bws_;

    single_tester()
        : ts_(ioc_, msg_)
        , pr_(capy_ctx_)
        , buf_(&s_)
        , brs_(ts_, pr_)
        , wts_(ioc_)
        , rts_(ioc_)
        , sr_(capy_ctx_)
        , res_(header_)
    {
        wts_.connect(rts_);
        pr_.reset();
        pr_.start();
        sr_.reset();
        auto srs = sr_.start_stream(res_);
        srs_capacity_ = srs.capacity();
        bws_.emplace(wts_, sr_, std::move(srs));
    }

    body_write_stream<test::stream>&
    bws()
    {
        return *bws_;
    }

    void
    async_read_some(std::size_t bs, system::error_code ec, std::size_t n)
    {
        brs_.async_read_some(buf_.prepare(bs), test_handler(ec, n));
    }

    void
    async_write_some(std::size_t bs, system::error_code ec, std::size_t n)
    {
        bws().async_write_some(buf_.prepare(bs), test_handler(ec, n));
    }

    void
    async_close(system::error_code ec)
    {
        bws().async_close(test_handler(ec, 0));
    }

    capy::const_buffer
    make_test_buffer(std::size_t size)
    {
        std::string val = body_.substr(0, size);
        val.resize(size, '.');
        boost::capy::string_buffer sb(&val);
        return sb.data();
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
        system::error_code ec_;
        std::size_t* written_;

        chunking_handler(
            system::error_code ec,
            std::size_t* written)
            : ec_(ec)
            , written_(written)
        {
        }

        void
        operator()(system::error_code ec, std::size_t n)
        {
            BOOST_TEST_EQ(ec, ec_);
            *written_ += n;
        }
    };

    // Ensure the edge case of being passed a zero-sized buffer works.
    void
    test_zero_sized_buffer()
    {
        // Ensure a read into a zero sized buffer returns with no error.
        std::string val;
        boost::capy::string_buffer sb(&val);
        auto cb = sb.data();
        bws().async_write_some(cb, test_handler(system::error_code{}, 0));
        test::run(ioc_);
    }

    // Test for a given buffer size (bs) and stream read size (cs)
    void
    test_with_chunking(std::size_t bs, std::size_t cs, int iters = 15)
    {
        wts_.write_size(cs); // Limit und stream write size to cs

        std::string finals;
        finals.reserve(bs * iters);

        std::size_t total = 0;
        std::size_t prev = 0;
        std::size_t writes = 0;
        for(int i = 0; i < iters; i++)
        {
            // Construct a buffer of size bs
            std::string val = body_.substr(0, bs);
            val.resize(bs, '.');
            boost::capy::string_buffer sb(&val);
            auto cb = sb.data();

            finals += val;

            // Calculate how many bytes we expect to read on each iteration
            // std::size_t expected = chunking_expected_n(bs, cs, (i == 0),
            // total);

            while(cb.size() > 0)
            {
                std::size_t emin = 1;
                std::size_t emax = std::min({ bs, srs_capacity_, cb.size() });

                std::size_t written = 0;
                bws().async_write_some(
                    cb,
                    chunking_handler(
                        system::error_code{},
                        &written));

                auto count = test::run(ioc_);
                BOOST_TEST_GE(count, 1);
                BOOST_TEST_LE(count, (size_t) 1 + header_length_ / cs);

                //std::cout << "count " << count << std::endl;

                total += written;
                cb += written;
                writes++;

                if (written < emin)
                {
                    std::cout << "err" << std::endl;
                }

                BOOST_TEST_GE(total, writes);
                BOOST_TEST_GE(written, emin);
                BOOST_TEST_LE(written, emax);

                BOOST_TEST_GE(rts_.nwrite_bytes() - prev, 1);

                prev = rts_.nwrite_bytes();

                BOOST_TEST_LE(rts_.nwrite_bytes(), bs * (i+1) + header_length_);

                BOOST_TEST(!sr_.is_done());
            }
        }

        BOOST_TEST_EQ(total, bs * iters);

        BOOST_TEST_LE(
            rts_.nwrite_bytes(), cs * writes + header_length_);

        bws().async_close(test::success_handler());

        auto count = test::run(ioc_);
        BOOST_TEST_GE(count, 1);

        BOOST_TEST_GT(rts_.nwrite_bytes(), iters + writes);
        BOOST_TEST_LE(rts_.nwrite_bytes(), cs * (writes + count) + header_length_);

        BOOST_TEST_EQ(rts_.nwrite_bytes(), bs * iters + header_length_);

        BOOST_TEST(sr_.is_done());

        BOOST_TEST(rts_.str() == header_ + finals);
    }

    void
    test_with_ignored_cancel_signal(std::size_t len)
    {
        std::string val = body_.substr(0, len);
        boost::capy::string_buffer sb(&val);
        auto cb = sb.data();

        // Add a signal to test cancellation
        asio::cancellation_signal c_signal;

        // First call: cancellation occurs after data is written to serializer.
        // The callback should receive success with the bytes written.
        // With the simplified loop, cancellation after successful write
        // is treated as success (the data was written).
        std::size_t expected_bytes = std::min(len, body_length_);
        bws().async_write_some(
            cb,
            asio::bind_cancellation_slot(
                c_signal.slot(),
                test_handler(system::error_code{}, expected_bytes)));

        // send a cancellation
        c_signal.emit(asio::cancellation_type::total);

        // Run up until the point of cancellation.
        test::run(ioc_);

        BOOST_TEST(!sr_.is_done());

        // Second call: write the remainder successfully.
        // Cancellation after successful write is not saved, so this succeeds.
        std::string remainder = body_.substr(len);
        boost::capy::string_buffer sb2(&remainder);
        auto cb2 = sb2.data();

        std::size_t remainder_len = body_length_ - len;
        bws().async_write_some(
            cb2,
            test_handler(system::error_code{}, remainder_len));

        test::run(ioc_);

        BOOST_TEST(!sr_.is_done());

        // Third call: close the stream and verify the message.
        bws().async_close(test_handler(system::error_code{}));

        test::run(ioc_);

        BOOST_TEST(sr_.is_done());
        BOOST_TEST(rts_.str() == msg_);
    }

    void
    test_asio_async_write(std::size_t cs, bool use_asio_buffer)
    {
        // limit chunk size on the underlying stream
        wts_.write_size(cs);

        if(use_asio_buffer)
        {
            asio::async_write(
                bws(),
                asio::buffer(body_.data(), body_.size()),
                test_handler(system::error_code{}, body_length_));
        }
        else
        {
            asio::async_write(
                bws(),
                capy::const_buffer(body_.data(), body_.size()),
                test_handler(system::error_code{}, body_length_));
        }

        test::run(ioc_);

        BOOST_TEST(!sr_.is_done());

        // Close the stream to flush remaining data
        bws().async_close(test_handler(system::error_code{}));

        test::run(ioc_);

        BOOST_TEST(sr_.is_done());
        BOOST_TEST(rts_.str() == msg_);
    }

    void
    test_stream_errors()
    {
        // Create a write test stream that fails on the first write.
        test::fail_count fc(0, asio::error::network_down);
        test::stream wts(ioc_, fc);
        test::stream rts(ioc_);
        wts.connect(rts);

        // Create a fresh serializer for this test
        http::serializer sr(capy_ctx_);
        sr.reset();
        http::response res(header_);

        // Create a new body_write_stream with the failing stream
        body_write_stream<test::stream> bws(wts, sr, sr.start_stream(res));

        // First call: data is committed to the serializer before the
        // stream write fails. Due to deferred error handling, this
        // returns success with the committed bytes, and saves the error.
        std::string val = body_;
        boost::capy::string_buffer sb(&val);
        auto cb = sb.data();

        bws.async_write_some(
            cb,
            test_handler(system::error_code{}, body_length_));

        // The operation completes with 1 handler invocation.
        BOOST_TEST_EQ(test::run(ioc_), 1);

        // Second call: receives the deferred error with 0 bytes.
        bws.async_write_some(
            cb,
            test_handler(asio::error::network_down, 0));

        // The deferred error is returned via async_immediate.
        BOOST_TEST_EQ(test::run(ioc_), 1);
    }

    void
    test_close_with_saved_error()
    {
        // Create a write test stream that fails on the first write.
        test::fail_count fc(0, asio::error::network_down);
        test::stream wts(ioc_, fc);
        test::stream rts(ioc_);
        wts.connect(rts);

        // Create a fresh serializer for this test
        http::serializer sr(capy_ctx_);
        sr.reset();
        http::response res(header_);

        // Create a new body_write_stream with the failing stream
        body_write_stream<test::stream> bws(wts, sr, sr.start_stream(res));

        // First call: data is committed to the serializer before the
        // stream write fails. Due to deferred error handling, this
        // returns success with the committed bytes, and saves the error.
        std::string val = body_;
        boost::capy::string_buffer sb(&val);
        auto cb = sb.data();

        bws.async_write_some(
            cb,
            test_handler(system::error_code{}, body_length_));

        // The operation completes with 1 handler invocation.
        BOOST_TEST_EQ(test::run(ioc_), 1);

        // async_close receives the saved error immediately.
        bws.async_close(test_handler(asio::error::network_down));

        // The deferred error is returned via async_immediate.
        BOOST_TEST_EQ(test::run(ioc_), 1);
    }

    void
    test_close_errors()
    {
        // Create a write test stream that fails on the second write.
        // The first write will succeed (writing body data), but the
        // close operation will fail when flushing remaining data.
        test::fail_count fc(1, asio::error::network_down);
        test::stream wts(ioc_, fc);
        test::stream rts(ioc_);
        wts.connect(rts);

        // Limit write size so data remains in serializer after first write.
        wts.write_size(1);

        // Create a fresh serializer for this test
        http::serializer sr(capy_ctx_);
        sr.reset();
        http::response res(header_);

        // Create a new body_write_stream with the failing stream
        body_write_stream<test::stream> bws(wts, sr, sr.start_stream(res));

        // Write body data - this should succeed.
        std::string val = body_;
        boost::capy::string_buffer sb(&val);
        auto cb = sb.data();

        bws.async_write_some(
            cb,
            test_handler(system::error_code{}, body_length_));

        BOOST_TEST_EQ(test::run(ioc_), 1);

        // Close the stream - this should fail when trying to flush
        // the remaining serializer data to the underlying stream.
        bws.async_close(test_handler(asio::error::network_down));

        BOOST_TEST_GE(test::run(ioc_), 1);
    }

    // Test cancellation during buffer-clearing loop (when bytes_ == 0).
    // This covers the case where the serializer buffer is full and we're
    // waiting for space, then get cancelled before any user data is copied.
    void
    test_cancel_during_buffer_clear()
    {
        wts_.write_size(1); // Very slow drain

        // First, fill the serializer's buffer completely by writing
        // data equal to its capacity
        std::size_t cap = srs_capacity_;
        std::string fill_data(cap, 'F');
        capy::const_buffer fill_cb(fill_data.data(), fill_data.size());

        bool fill_complete = false;
        bws().async_write_some(
            fill_cb,
            [&](system::error_code, std::size_t)
            {
                fill_complete = true;
            });

        test::run(ioc_);
        BOOST_TEST(fill_complete);

        // Now the buffer should be full. The next write should enter
        // the buffer-clearing loop with bytes_ == 0 on the first iteration.
        std::string more_data(64, 'X');
        capy::const_buffer cb(more_data.data(), more_data.size());

        asio::cancellation_signal c_signal;

        system::error_code result_ec;
        std::size_t result_bytes = 0;

        bws().async_write_some(
            cb,
            asio::bind_cancellation_slot(
                c_signal.slot(),
                [&](system::error_code ec, std::size_t n)
                {
                    result_ec = ec;
                    result_bytes = n;
                }));

        // Emit cancellation immediately - we should be in the loop
        // with bytes_ == 0 because the buffer is full
        c_signal.emit(asio::cancellation_type::total);

        // Let the operation complete
        test::run(ioc_);

        // Should complete with operation_aborted and 0 bytes
        // because cancellation occurred while bytes_ == 0
        BOOST_TEST_EQ(result_ec, asio::error::operation_aborted);
        BOOST_TEST_EQ(result_bytes, 0u);
    }
};

// Result type for async write operations
struct write_result
{
    system::error_code ec;
    std::size_t bytes_transferred;
};

// Helper to wrap async_write_some for coroutines
template<class Stream, class ConstBufferSequence>
capy::async_op<write_result>
coro_write_some(
    body_write_stream<Stream>& bws,
    ConstBufferSequence const& buffers)
{
    return capy::make_async_op<write_result>(
        bws.async_write_some(buffers, asio::deferred));
}

// Helper to wrap async_close for coroutines
template<class Stream>
capy::async_op<system::error_code>
coro_close(body_write_stream<Stream>& bws)
{
    return capy::make_async_op<system::error_code>(
        bws.async_close(asio::deferred));
}

capy::task<void>
do_coro_write(
    test::stream& wts,
    test::stream& rts,
    http::serializer& sr,
    http::serializer::stream srs,
    std::string const& body,
    std::string const& expected_msg)
{
    body_write_stream<test::stream> bws(wts, sr, std::move(srs));

    // Write body data using co_await
    capy::const_buffer cb(body.data(), body.size());
    std::size_t total_written = 0;

    while(cb.size() > 0)
    {
        auto result = co_await coro_write_some(bws, cb);
        BOOST_TEST(!result.ec.failed());
        BOOST_TEST_GT(result.bytes_transferred, 0u);
        total_written += result.bytes_transferred;
        cb += result.bytes_transferred;
    }

    BOOST_TEST_EQ(total_written, body.size());
    BOOST_TEST(!sr.is_done());

    // Close the stream
    auto ec = co_await coro_close(bws);
    BOOST_TEST(!ec.failed());

    BOOST_TEST(sr.is_done());
    BOOST_TEST_EQ(rts.str(), expected_msg);

    co_return;
}

void
test_coroutine()
{
    // Set up context with parser and serializer services
    capy::polystore capy_ctx;
    http::install_parser_service(capy_ctx, {});
    http::install_serializer_service(capy_ctx, {});

    std::string body = "Hello World!";
    std::string header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Length: 12\r\n"
        "\r\n";
    std::string expected_msg = header + body;

    asio::io_context ioc;

    test::stream wts(ioc);
    test::stream rts(ioc);
    wts.connect(rts);

    http::serializer sr(capy_ctx);
    sr.reset();

    http::response res(header);
    auto srs = sr.start_stream(res);

    // Launch coroutine using async_run (default handler rethrows exceptions)
    capy::async_run(wrap_executor(ioc.get_executor()))(
        do_coro_write(wts, rts, sr, std::move(srs), body, expected_msg));

    ioc.run();
}

} // anonymous namespace.

struct body_write_stream_test
{
    void
    run()
    {
        // Read into a zero sized buffer should return immediately without error
        {
            single_tester().test_zero_sized_buffer();
        }

        // async_read_some reads the body for various chunk
        // sizes.
        {
            int sizes[] = { 1, 2, 13, 1597, 100'000 };
            // Iterate through buffer sizes
            for(std::size_t bs: sizes)
            {
                // Iterate through chunk sizes
                for(std::size_t cs : sizes)
                {
                    if(cs > bs / 10000)
                    {
                        auto iters = static_cast<int>(
                            std::min((size_t)10, (cs / bs) + 1));
                        single_tester().test_with_chunking(bs, cs, iters);
                    }
                }
            }
        }

        // Test async_write_some with ignored cancellation signal
        {
            // Iterate through different amounts of data written before
            // cancellation. Only go up to body_length_ since that's the
            // maximum useful data.
            std::size_t body_len = single_tester().body_length_;
            for(std::size_t len = 1; len <= body_len; len++)
            {
                single_tester().test_with_ignored_cancel_signal(len);
            }
        }

        // Test asio::async_write works with body_write_stream
        {
            // pick a representative chunk size
            std::size_t cs = 5;

            // Perform the test using the Boost Buffers buffer directly
            single_tester().test_asio_async_write(cs, false);
            // And again using an asio buffer wrapper
            single_tester().test_asio_async_write(cs, true);
        }

        // async_write_some reports stream errors
        {
            single_tester().test_stream_errors();
        }

        // async_close reports saved errors
        {
            single_tester().test_close_with_saved_error();
        }

        // async_close reports stream errors during flush
        {
            single_tester().test_close_errors();
        }

        // Test cancellation during buffer-clearing loop
        {
            single_tester().test_cancel_during_buffer_clear();
        }

        // Test C++20 coroutine compatibility
        {
            test_coroutine();
        }
    }
};

TEST_SUITE(body_write_stream_test, "boost.beast2.body_write_stream");

} // beast2
} // boost
