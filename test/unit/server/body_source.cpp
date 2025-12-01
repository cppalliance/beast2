//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

// Test that header file is self-contained.
#include <boost/beast2/server/body_source.hpp>

#include <boost/core/detail/static_assert.hpp>
#include <type_traits>

#include "test_suite.hpp"

namespace boost {
namespace beast2 {

namespace {

struct data_source
{
    core::string_view s_;

    data_source(
        core::string_view s)
        : s_(s)
    {
    }

    buffers::const_buffer
    data() const
    {
        return { s_.data(), s_.size() };
    }
};

struct read_source
{
    core::string_view s_;
    std::size_t nread_ = 0;
    system::error_code ec_;

    explicit read_source(
        core::string_view s,
        system::error_code ec = {})
        : s_(s)
        , ec_(ec)
    {
    }

    void rewind()
    {
        nread_ = 0;
    }

    template<class MutableBufferSequence>
    std::size_t read(
        MutableBufferSequence const& dest,
        system::error_code& ec)
    {
        if( nread_ > 0 &&
            ec_.failed())
        {
            // fail on second read
            ec = ec_;
            return 0;
        }
        auto n = buffers::copy(
            dest,
            buffers::const_buffer(
                s_.data() + nread_,
                s_.size() - nread_));
        nread_ += n;
        if(nread_ >= s_.size())
        {
            if(ec_.failed())
                ec = ec_;
            else
                ec = http_proto::error::end_of_stream;
        }
        else
        {
            ec = {};
        }
        return n;
    }
};

} // (anon)

BOOST_CORE_STATIC_ASSERT(
    std::is_move_constructible<body_source>::value);
BOOST_CORE_STATIC_ASSERT(
    ! std::is_copy_constructible<body_source>::value);
BOOST_CORE_STATIC_ASSERT(
    ! std::is_constructible<body_source, body_source const&>::value);
BOOST_CORE_STATIC_ASSERT(
    std::is_constructible<body_source, data_source>::value);
BOOST_CORE_STATIC_ASSERT(
    std::is_constructible<body_source, read_source>::value);
BOOST_CORE_STATIC_ASSERT(
    std::is_move_assignable<body_source>::value);
BOOST_CORE_STATIC_ASSERT(
    ! std::is_copy_assignable<body_source>::value);
BOOST_CORE_STATIC_ASSERT(
    std::is_assignable<body_source, body_source>::value);
BOOST_CORE_STATIC_ASSERT(
    ! std::is_assignable<body_source, body_source const&>::value);
BOOST_CORE_STATIC_ASSERT(
    std::is_assignable<body_source, data_source>::value);
BOOST_CORE_STATIC_ASSERT(
    std::is_assignable<body_source, read_source>::value);

struct body_source_test
{
    void grind(
        body_source& b,
        core::string_view s0,
        system::error_code fec = {})
    {
        char buf[16];
        for(std::size_t n = 1; n <= sizeof(buf); ++n)
        {
            std::string s;
            system::error_code ec;
            b.rewind();
            for(;;)
            {
                auto nread = b.read(buf, n, ec);
                s.append(buf, nread);
                if(ec == http_proto::error::end_of_stream)
                {
                    BOOST_TEST(! fec.failed());
                    BOOST_TEST_EQ(s, s0);
                    break;
                }
                if(ec.failed())
                {
                    BOOST_TEST_EQ(ec, fec);
                    break;
                }
                BOOST_TEST_GT(nread, 0);
            }
        }
    }

    void testEmpty()
    {
        body_source b;
        BOOST_TEST_EQ(b.has_size(), true);
        BOOST_TEST_EQ(b.size(), 0);
        BOOST_TEST_EQ(b.has_buffers(), true);
        BOOST_TEST_EQ(buffers::size(b.data()), 0);
        BOOST_TEST_NO_THROW(b.rewind());
        grind(b, "");
    }

    void testBuffers()
    {
        core::string_view s1("Hello, world!");
        core::string_view s2("Boost");

        body_source b1((data_source(s1)));
        BOOST_TEST_EQ(b1.has_size(), true);
        BOOST_TEST_EQ(b1.size(), s1.size());
        BOOST_TEST_EQ(b1.has_buffers(), true);
        BOOST_TEST_EQ(buffers::size(b1.data()), s1.size());
        BOOST_TEST_NO_THROW(b1.rewind());
        grind(b1, s1);

        body_source b2 = std::move(b1);
        BOOST_TEST_EQ(b2.has_size(), true);
        BOOST_TEST_EQ(b2.size(), s1.size());
        BOOST_TEST_EQ(b2.has_buffers(), true);
        BOOST_TEST_EQ(buffers::size(b2.data()), s1.size());
        BOOST_TEST_NO_THROW(b2.rewind());
        grind(b2, s1);

        b1 = data_source(s2);
        BOOST_TEST_EQ(b1.has_size(), true);
        BOOST_TEST_EQ(b1.size(), s2.size());
        BOOST_TEST_EQ(b1.has_buffers(), true);
        BOOST_TEST_EQ(buffers::size(b1.data()), s2.size());
        BOOST_TEST_NO_THROW(b1.rewind());
        grind(b1, s2);
    }

    void testStream()
    {
        core::string_view s1("Hello, world!");
        core::string_view s2("Boost");

        body_source b1((read_source(s1)));
        BOOST_TEST_EQ(b1.has_size(), false);
        BOOST_TEST_EQ(b1.has_buffers(), false);
        BOOST_TEST_THROWS(b1.size(), std::invalid_argument);
        BOOST_TEST_THROWS(b1.data(), std::invalid_argument);
        BOOST_TEST_NO_THROW(b1.rewind());
        grind(b1, s1);

        body_source b2 = std::move(b1);
        BOOST_TEST_EQ(b2.has_size(), false);
        BOOST_TEST_EQ(b2.has_buffers(), false);
        BOOST_TEST_THROWS(b2.size(), std::invalid_argument);
        BOOST_TEST_THROWS(b2.data(), std::invalid_argument);
        BOOST_TEST_NO_THROW(b2.rewind());
        BOOST_TEST_EQ(b1.has_size(), true);
        BOOST_TEST_EQ(b1.size(), 0);
        BOOST_TEST_EQ(b1.has_buffers(), true);
        BOOST_TEST_EQ(buffers::size(b1.data()), 0);
        BOOST_TEST_NO_THROW(b1.rewind());
        grind(b2, s1);

        b1 = read_source(s2);
        BOOST_TEST_EQ(b1.has_size(), false);
        BOOST_TEST_EQ(b1.has_buffers(), false);
        BOOST_TEST_THROWS(b1.size(), std::invalid_argument);
        BOOST_TEST_THROWS(b1.data(), std::invalid_argument);
        BOOST_TEST_NO_THROW(b1.rewind());
        grind(b1, s2);

        // sized source
        b2 = body_source(s2.size(), read_source(s2));
        BOOST_TEST_EQ(b2.has_size(), true);
        BOOST_TEST_EQ(b2.has_buffers(), false);
        BOOST_TEST_EQ(b2.size(), s2.size());
        BOOST_TEST_THROWS(b1.data(), std::invalid_argument);
        BOOST_TEST_NO_THROW(b2.rewind());
        grind(b2, s2);
    }

    void testFail()
    {
        core::string_view s1("Hello, world!");
        system::error_code fec =
            http_proto::error::bad_connection;

        body_source b1((read_source(s1, fec)));
        BOOST_TEST_EQ(b1.has_size(), false);
        BOOST_TEST_EQ(b1.has_buffers(), false);
        BOOST_TEST_THROWS(b1.size(), std::invalid_argument);
        BOOST_TEST_THROWS(b1.data(), std::invalid_argument);
        BOOST_TEST_NO_THROW(b1.rewind());
        grind(b1, s1, fec);
    }

    void run()
    {
        testEmpty();
        testBuffers();
        testStream();
        testFail();
    }
};

TEST_SUITE(
    body_source_test,
    "boost.beast2.body_source");

} // beast2
} // boost
