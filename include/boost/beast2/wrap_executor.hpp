//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_WRAP_EXECUTOR_HPP
#define BOOST_BEAST2_WRAP_EXECUTOR_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/capy/executor.hpp>
#include <boost/asio/associated_allocator.hpp>
#include <boost/asio/post.hpp>
#include <cstddef>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace boost {
namespace beast2 {

namespace detail {

// Metadata stored before each work allocation
struct work_metadata
{
    void* raw;
    std::size_t total_size;
};

template<class AsioExecutor>
struct asio_executor_impl
{
    friend struct capy::executor::access;

    using allocator_type = typename
        asio::associated_allocator<AsioExecutor>::type;

    AsioExecutor exec_;
    allocator_type alloc_;

    explicit
    asio_executor_impl(AsioExecutor ex)
        : exec_(std::move(ex))
        , alloc_(asio::get_associated_allocator(exec_))
    {
    }

    // Move constructor
    asio_executor_impl(asio_executor_impl&& other) noexcept
        : exec_(std::move(other.exec_))
        , alloc_(std::move(other.alloc_))
    {
    }

    // Move assignment
    asio_executor_impl& operator=(asio_executor_impl&& other) noexcept
    {
        if(this != &other)
        {
            exec_ = std::move(other.exec_);
            alloc_ = std::move(other.alloc_);
        }
        return *this;
    }

    // Delete copy operations
    asio_executor_impl(asio_executor_impl const&) = delete;
    asio_executor_impl& operator=(asio_executor_impl const&) = delete;

private:
    void*
    allocate(std::size_t size, std::size_t align)
    {
        // Rebind allocator to char for byte-level allocation
        using char_alloc = typename std::allocator_traits<
            allocator_type>::template rebind_alloc<char>;
        char_alloc a(alloc_);

        // We need space for:
        // - metadata struct (aligned to work_metadata alignment)
        // - padding for work alignment
        // - work object
        std::size_t const meta_size = sizeof(work_metadata);
        std::size_t const total_size = meta_size + align + size;

        // Allocate raw storage
        char* raw = std::allocator_traits<char_alloc>::allocate(a, total_size);

        // Compute aligned pointer for work after metadata
        char* after_meta = raw + meta_size;
        void* aligned = after_meta;
        std::size_t space = total_size - meta_size;
        aligned = std::align(align, size, aligned, space);

        // Store metadata immediately before the aligned work region
        work_metadata* meta = reinterpret_cast<work_metadata*>(
            static_cast<char*>(aligned) - sizeof(work_metadata));
        meta->raw = raw;
        meta->total_size = total_size;

        return aligned;
    }

    void
    deallocate(void* p, std::size_t /*size*/, std::size_t /*align*/)
    {
        using char_alloc = typename std::allocator_traits<
            allocator_type>::template rebind_alloc<char>;
        char_alloc a(alloc_);

        // Retrieve metadata stored before p
        work_metadata* meta = reinterpret_cast<work_metadata*>(
            static_cast<char*>(p) - sizeof(work_metadata));

        std::allocator_traits<char_alloc>::deallocate(
            a, static_cast<char*>(meta->raw), meta->total_size);
    }

    void
    submit(capy::executor::work* w)
    {
        // Capture a copy of allocator for the lambda
        allocator_type alloc_copy = alloc_;
        asio::post(exec_,
            [w, alloc_copy]() mutable
            {
                using char_alloc = typename std::allocator_traits<
                    allocator_type>::template rebind_alloc<char>;
                char_alloc a(alloc_copy);

                // Retrieve metadata stored before w
                work_metadata* meta = reinterpret_cast<work_metadata*>(
                    reinterpret_cast<char*>(w) - sizeof(work_metadata));
                void* raw = meta->raw;
                std::size_t total_size = meta->total_size;

                w->invoke();
                w->~work();

                std::allocator_traits<char_alloc>::deallocate(
                    a, static_cast<char*>(raw), total_size);
            });
    }
};

} // detail

/** Return a capy::executor from an Asio executor.

    This function wraps an Asio executor in a capy::executor,
    mapping the capy::executor implementation API to the
    corresponding Asio executor operations.

    The returned executor uses get_associated_allocator on
    the Asio executor to obtain the allocator for work items.

    @param ex The Asio executor to wrap.

    @return A capy::executor that submits work via the
    provided Asio executor.
*/
template<class AsioExecutor>
capy::executor
wrap_executor(AsioExecutor ex)
{
    return capy::executor::wrap(
        detail::asio_executor_impl<
            typename std::decay<AsioExecutor>::type>(
                std::move(ex)));
}

} // beast2
} // boost

#endif

