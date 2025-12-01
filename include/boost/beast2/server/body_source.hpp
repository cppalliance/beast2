//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_BODY_SOURCE_HPP
#define BOOST_BEAST2_SERVER_BODY_SOURCE_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/detail/except.hpp>
#include <boost/beast2/detail/type_traits.hpp>
#include <boost/http_proto/error.hpp>
#include <boost/buffers/buffer.hpp>
#include <boost/buffers/copy.hpp>
#include <boost/buffers/slice.hpp>
#include <boost/buffers/data_source.hpp>
#include <boost/buffers/read_source.hpp>
#include <boost/core/span.hpp>
#include <boost/system/error_code.hpp>

namespace boost {
namespace beast2 {

/** Tag for customizing body construction
*/
struct construct_body_tag {};

//-----------------------------------------------

/** A type erased HTTP message body.

    This type erases the specific body type used to represent the message body.
    It provides a uniform interface for reading the body data regardless of
    how the body is implemented. Accessing the bytes is achieved by calling
    @ref read which reads data into a caller-provided buffer. Alternatively,
    when @ref has_buffers returns `true` the body consists of buffers in memory,
    and they can be accessed directly by calling @ref get_buffers.

    Example bodies include:
    - in-memory buffers
    - file-backed bodies
    - generated bodies

    @note A body_source is movable but not copyable.

    Type-erased bodies can always be rewound to the beginning by calling
    @ref rewind. Therefore, bodies can be read multiple times.

    @par Thread Safety
    Unsafe.
*/
class body_source
{
public:
    /** Destructor.
    */
    BOOST_BEAST2_DECL ~body_source();

    /** Constructor

        Default-constructed bodies are empty.
    */
    body_source() = default;

    /** Constructor

        The content of @p other is transferred to `*this`. The
        moved-from body is left empty, as if default-constructed.
    */
    body_source(body_source&& other) noexcept
        : impl_(other.impl_)
    {
        other.impl_ = nullptr;
    }

    /** Assignment

        The previous body, if any, is released and the
        content of @p other is transferred to `*this`.
    */
    BOOST_BEAST2_DECL
    body_source& operator=(body_source&& other) noexcept;

    /** Construct a streaming body source.
    */
    template<class ReadSource
        , typename std::enable_if<
            ! std::is_same<typename std::decay<ReadSource>::type, body_source>::value &&
            buffers::is_read_source<typename std::decay<ReadSource>::type>::value
        , int>::type = 0>
    body_source(ReadSource&& body);

    /** Construct a streaming body source with a known size.
    */
    template<class ReadSource
        , typename std::enable_if<
            ! std::is_same<typename std::decay<ReadSource>::type, body_source>::value &&
            buffers::is_read_source<typename std::decay<ReadSource>::type>::value
        , int>::type = 0>
    body_source(
        std::size_t known_size,
        ReadSource&& body);

    /** Construct a buffers body source.
    */
    template<class DataSource
        , typename std::enable_if<
            ! std::is_same<typename std::decay<DataSource>::type, body_source>::value &&
            buffers::is_data_source<typename std::decay<DataSource>::type>::value
        , int>::type = 0>
    body_source(DataSource&& body);

    //template<class T>
    //body(T&& t);

    /** Return `true` if the size of the body is known.
    */
    bool has_size() const noexcept
    {
        if(impl_) 
            return impl_->has_size();
        return true; // empty
    }

    /** Return `true` if the body consists of buffers in memory.
        When the body consists of buffers in memory, they can
        also be accessed directly using @ref get_buffers.
    */
    bool has_buffers() const noexcept
    {
        if(impl_)
            return impl_->has_buffers();
        return true; // empty
    }

    /** Return the size of the body, if available.
        @throw std::invalid_argument if @ref has_size returns `false`.
        @return The size of the body in bytes.
    */
    auto size() const -> std::size_t
    {
        if(impl_)
            return impl_->size();
        return 0; // empty
    }

    /** Return the buffers representing the body, if available.
        @throw std::invalid_argument if @ref has_buffers returns `false`.
        @return A span of buffers representing the body.
    */
    auto data() const ->
        span<buffers::const_buffer const>
    {
        if(impl_)
            return impl_->data();
        return {}; // empty
    }

    /** Rewind the body to the beginning.
        This allows the body to be accessed from the start when calling @read.
    */
    void rewind()
    {
        if(impl_)
            return impl_->rewind();
        // empty
    }

    /** Read from the body into a caller-provided buffer.

        @param dest A pointer to the buffer to read into.
        @param n The maximum number of bytes to read.
        @param ec Set to the error, if any occurred.
        @return The number of bytes read, which may be
        less than the number requested. A return value of
        zero indicates end-of-body.
    */
    auto
    read(void* dest, std::size_t n,
        system::error_code& ec) ->
            std::size_t
    {
        if(impl_)
            return impl_->read(dest, n, ec);
        // empty
        ec = http_proto::error::end_of_stream;
        return 0;
    }

private:
    struct impl
    {
        virtual ~impl() = default;
        virtual bool has_size() const noexcept { return false; }
        virtual bool has_buffers() const noexcept { return false; }
        virtual std::size_t size() const
        {
            detail::throw_invalid_argument();
        }
        virtual auto data() const ->
            span<buffers::const_buffer const>
        {
            detail::throw_invalid_argument();
        }
        virtual void rewind() = 0;
        virtual std::size_t read(
            void* dest, std::size_t n,
            system::error_code& ec) = 0;
    };

    impl* impl_ = nullptr;
};

//-----------------------------------------------

template<class ReadSource
    , typename std::enable_if<
        ! std::is_same<typename std::decay<ReadSource>::type, body_source>::value &&
        buffers::is_read_source<typename std::decay<ReadSource>::type>::value
    , int>::type>
body_source::
body_source(
    ReadSource&& body)
{
    struct model : impl
    {
        system::error_code ec_;
        typename std::decay<ReadSource>::type body_;

        explicit model(ReadSource&& body)
            : body_(std::forward<ReadSource>(body))
        {
        }

        void rewind() override
        {
            ec_ = {};
            body_.rewind();
        }

        std::size_t read(
            void* dest,
            std::size_t size,
            system::error_code& ec) override
        {
            if(ec_.failed())
            {
                ec = ec_;
                return 0;
            }
            auto nread = body_.read(
                buffers::mutable_buffer(dest, size), ec);
            ec_ = ec;
            return nread;
        }
    };

    auto p = ::operator new(sizeof(model));
    impl_ = ::new(p) model(
        std::forward<ReadSource>(body));
}

/** Construct a streaming body source with a known size.
*/
template<class ReadSource
    , typename std::enable_if<
        ! std::is_same<typename std::decay<ReadSource>::type, body_source>::value &&
        buffers::is_read_source<typename std::decay<ReadSource>::type>::value
    , int>::type>
body_source::
body_source(
    std::size_t known_size,
    ReadSource&& body)
{
    struct model : impl
    {
        std::size_t size_;
        system::error_code ec_;
        typename std::decay<ReadSource>::type body_;

        model(
            ReadSource&& body,
            std::size_t known_size)
            : size_(known_size)
            , body_(std::forward<ReadSource>(body))
        {
        }

        bool has_size() const noexcept override
        {
            return true;
        }

        std::size_t size() const override
        {
            return size_;
        }

        void rewind() override
        {
            ec_ = {};
            body_.rewind();
        }

        std::size_t read(
            void* dest,
            std::size_t size,
            system::error_code& ec) override
        {
            if(ec_.failed())
            {
                ec = ec_;
                return 0;
            }
            auto nread = body_.read(
                buffers::mutable_buffer(dest, size), ec);
            ec_ = ec;
            return nread;
        }
    };

    auto p = ::operator new(sizeof(model));
    impl_ = ::new(p) model(
        std::forward<ReadSource>(body), known_size);
}

/** Construct a buffers body source.
*/
template<class DataSource
    , typename std::enable_if<
        ! std::is_same<typename std::decay<DataSource>::type, body_source>::value &&
        buffers::is_data_source<typename std::decay<DataSource>::type>::value
    , int>::type>
body_source::
body_source(
    DataSource&& body)
{
    struct model : impl
    {
        typename std::decay<DataSource>::type body_;
        std::size_t size_ = 0;
        span<boost::buffers::const_buffer> bs_;
        std::size_t nread_ = 0;

        explicit model(
            DataSource&& body) noexcept
            : body_(std::forward<DataSource>(body))
        {
            auto const& data = body_.data();
            auto const& end = buffers::end(data);
            auto p = reinterpret_cast<
                buffers::const_buffer*>(this+1);
            std::size_t length = 0;
            for(auto it = buffers::begin(data); it != end; ++it)
            {
                boost::buffers::const_buffer cb(*it);
                size_ += cb.size();
                *p++ = cb;
                ++length;
            }
            bs_ = { reinterpret_cast<
                buffers::const_buffer*>(this + 1), length };
        }

        bool has_size() const noexcept override
        {
            return true;
        }

        bool has_buffers() const noexcept override
        {
            return true;
        }

        std::size_t size() const override
        {
            return size_;
        }

        span<buffers::const_buffer const>
        data() const override
        {
            return bs_;
        }

        void rewind() override
        {
            nread_ = 0;
        }

        std::size_t read(
            void* dest,
            std::size_t n0,
            system::error_code& ec) override
        {
            std::size_t n = buffers::copy(
                buffers::mutable_buffer(dest, n0),
                buffers::sans_prefix(bs_, nread_));
            nread_ += n;
            if(nread_ >= size_)
                ec = http_proto::error::end_of_stream;
            else
                ec = {};
            return n;
        }
    };

    std::size_t length = 0;
    auto const& data = body.data();
    auto const& end = buffers::end(data);
    for(auto it = buffers::begin(data);
        it != end; ++it)
    {
        ++length;
    }

    // VFALCO this requires DataSource to be nothrow
    // move constructible for strong exception safety.
    auto p = ::operator new(sizeof(model) +
        length * sizeof(buffers::const_buffer));
    impl_ = ::new(p) model(
        std::forward<DataSource>(body));
}

} // beast2
} // boost

#endif
