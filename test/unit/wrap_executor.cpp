//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

// Test that header file is self-contained.
#include <boost/beast2/wrap_executor.hpp>

#include "test_helpers.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <atomic>
#include <memory>

namespace boost {
namespace beast2 {

//-----------------------------------------------------------------------------

/** Tracking allocator to verify allocator usage.
*/
template<class T>
struct tracking_allocator
{
    using value_type = T;

    std::shared_ptr<std::atomic<int>> alloc_count;
    std::shared_ptr<std::atomic<int>> dealloc_count;

    tracking_allocator()
        : alloc_count(std::make_shared<std::atomic<int>>(0))
        , dealloc_count(std::make_shared<std::atomic<int>>(0))
    {
    }

    template<class U>
    tracking_allocator(tracking_allocator<U> const& other)
        : alloc_count(other.alloc_count)
        , dealloc_count(other.dealloc_count)
    {
    }

    T* allocate(std::size_t n)
    {
        ++(*alloc_count);
        return static_cast<T*>(::operator new(n * sizeof(T)));
    }

    void deallocate(T* p, std::size_t)
    {
        ++(*dealloc_count);
        ::operator delete(p);
    }

    template<class U>
    bool operator==(tracking_allocator<U> const& other) const
    {
        return alloc_count == other.alloc_count;
    }

    template<class U>
    bool operator!=(tracking_allocator<U> const& other) const
    {
        return !(*this == other);
    }
};

/** Executor wrapper that associates an allocator.
*/
template<class Executor, class Allocator>
struct executor_with_allocator
{
    Executor exec_;
    Allocator alloc_;

    using inner_executor_type = Executor;

    executor_with_allocator(
        Executor ex,
        Allocator alloc)
        : exec_(std::move(ex))
        , alloc_(std::move(alloc))
    {
    }

    Executor const& get_inner_executor() const noexcept
    {
        return exec_;
    }

    Allocator get_allocator() const noexcept
    {
        return alloc_;
    }

    // Forward executor operations
    auto context() const noexcept -> decltype(exec_.context())
    {
        return exec_.context();
    }

    void on_work_started() const noexcept
    {
        exec_.on_work_started();
    }

    void on_work_finished() const noexcept
    {
        exec_.on_work_finished();
    }

    template<class F, class A>
    void dispatch(F&& f, A const& a) const
    {
        exec_.dispatch(std::forward<F>(f), a);
    }

    template<class F, class A>
    void post(F&& f, A const& a) const
    {
        exec_.post(std::forward<F>(f), a);
    }

    template<class F, class A>
    void defer(F&& f, A const& a) const
    {
        exec_.defer(std::forward<F>(f), a);
    }

    bool operator==(executor_with_allocator const& other) const noexcept
    {
        return exec_ == other.exec_;
    }

    bool operator!=(executor_with_allocator const& other) const noexcept
    {
        return exec_ != other.exec_;
    }
};

} // beast2

// Specialize associated_allocator for our wrapper
namespace asio {

template<class Executor, class Allocator, class DefaultAllocator>
struct associated_allocator<
    beast2::executor_with_allocator<Executor, Allocator>,
    DefaultAllocator>
{
    using type = Allocator;

    static type get(
        beast2::executor_with_allocator<Executor, Allocator> const& ex,
        DefaultAllocator const& = DefaultAllocator()) noexcept
    {
        return ex.get_allocator();
    }
};

} // asio

namespace beast2 {

//-----------------------------------------------------------------------------

struct wrap_executor_test
{
    void
    testWrapExecutor()
    {
        asio::io_context ioc;
        capy::executor ex = wrap_executor(ioc.get_executor());
        BOOST_TEST(static_cast<bool>(ex));
    }

    void
    testPostLambda()
    {
        asio::io_context ioc;
        capy::executor ex = wrap_executor(ioc.get_executor());

        bool called = false;
        ex.post([&called]{ called = true; });

        BOOST_TEST(!called);
        test::run(ioc);
        BOOST_TEST(called);
    }

    void
    testPostMultiple()
    {
        asio::io_context ioc;
        capy::executor ex = wrap_executor(ioc.get_executor());

        int count = 0;
        ex.post([&count]{ ++count; });
        ex.post([&count]{ ++count; });
        ex.post([&count]{ ++count; });

        BOOST_TEST_EQ(count, 0);
        test::run(ioc);
        BOOST_TEST_EQ(count, 3);
    }

    void
    testPostWithCapture()
    {
        asio::io_context ioc;
        capy::executor ex = wrap_executor(ioc.get_executor());

        int result = 0;
        int a = 10, b = 20;
        ex.post([&result, a, b]{ result = a + b; });

        test::run(ioc);
        BOOST_TEST_EQ(result, 30);
    }

    void
    testPostWithMoveOnlyCapture()
    {
        struct callable
        {
            int& result;
            std::unique_ptr<int> ptr;

            void operator()()
            {
                result = *ptr;
            }
        };

        asio::io_context ioc;
        capy::executor ex = wrap_executor(ioc.get_executor());

        int result = 0;
        std::unique_ptr<int> ptr(new int(42));
        ex.post(callable{result, std::move(ptr)});

        test::run(ioc);
        BOOST_TEST_EQ(result, 42);
    }

    void
    testCopyExecutor()
    {
        asio::io_context ioc;
        capy::executor exec1 = wrap_executor(ioc.get_executor());
        capy::executor exec2 = exec1;

        int count = 0;
        exec1.post([&count]{ ++count; });
        exec2.post([&count]{ ++count; });

        test::run(ioc);
        BOOST_TEST_EQ(count, 2);
    }

    void
    testMoveExecutor()
    {
        asio::io_context ioc;
        capy::executor exec1 = wrap_executor(ioc.get_executor());
        capy::executor exec2 = std::move(exec1);

        bool called = false;
        exec2.post([&called]{ called = true; });

        test::run(ioc);
        BOOST_TEST(called);
    }

    void
    testWithStrand()
    {
        asio::io_context ioc;
        asio::strand<asio::io_context::executor_type> strand(
            ioc.get_executor());
        capy::executor ex = wrap_executor(strand);

        int count = 0;
        ex.post([&count]{ ++count; });
        ex.post([&count]{ ++count; });

        test::run(ioc);
        BOOST_TEST_EQ(count, 2);
    }

    void
    testAssociatedAllocator()
    {
        asio::io_context ioc;
        tracking_allocator<char> alloc;

        executor_with_allocator<
            asio::io_context::executor_type,
            tracking_allocator<char>> wrapped_exec(
                ioc.get_executor(), alloc);

        capy::executor ex = wrap_executor(wrapped_exec);

        bool called = false;
        ex.post([&called]{ called = true; });

        // Before running, allocation should have happened
        BOOST_TEST_GT(*alloc.alloc_count, 0);

        test::run(ioc);
        BOOST_TEST(called);

        // After running, deallocation should have happened
        BOOST_TEST_GT(*alloc.dealloc_count, 0);
        BOOST_TEST_EQ(*alloc.alloc_count, *alloc.dealloc_count);
    }

    void
    testAsyncPostNonVoid()
    {
        asio::io_context ioc;
        capy::executor ex = wrap_executor(ioc.get_executor());

        int result = 0;
        bool handler_called = false;

        ex.submit(
            []{ return 42; },
            [&](system::result<int, std::exception_ptr> r)
            {
                handler_called = true;
                if(r.has_value())
                    result = r.value();
            });

        test::run(ioc);
        BOOST_TEST(handler_called);
        BOOST_TEST_EQ(result, 42);
    }

    void
    testAsyncPostVoid()
    {
        asio::io_context ioc;
        capy::executor ex = wrap_executor(ioc.get_executor());

        bool work_called = false;
        bool handler_called = false;

        ex.submit(
            [&work_called]{ work_called = true; },
            [&handler_called](system::result<void, std::exception_ptr>)
            {
                handler_called = true;
            });

        test::run(ioc);
        BOOST_TEST(work_called);
        BOOST_TEST(handler_called);
    }

    void
    testAsyncPostException()
    {
        asio::io_context ioc;
        capy::executor ex = wrap_executor(ioc.get_executor());

        bool handler_called = false;
        bool got_exception = false;

        ex.submit(
            []() -> int { throw std::runtime_error("test"); },
            [&](system::result<int, std::exception_ptr> r)
            {
                handler_called = true;
                if(r.has_error())
                    got_exception = true;
            });

        test::run(ioc);
        BOOST_TEST(handler_called);
        BOOST_TEST(got_exception);
    }

    // Test that mimics route_params storage pattern
    void
    testStoredInStruct()
    {
        struct params
        {
            capy::executor ex;
        };

        asio::io_context ioc;
        params p;
        p.ex = wrap_executor(ioc.get_executor());

        bool called = false;
        p.ex.post([&called]{ called = true; });

        test::run(ioc);
        BOOST_TEST(called);
    }

    // Test executor survives struct move
    void
    testStoredInStructAfterMove()
    {
        struct params
        {
            capy::executor ex;
            int dummy = 0;
        };

        asio::io_context ioc;
        params p1;
        p1.ex = wrap_executor(ioc.get_executor());

        // Move the struct
        params p2 = std::move(p1);

        bool called = false;
        p2.ex.post([&called]{ called = true; });

        test::run(ioc);
        BOOST_TEST(called);
    }

    // Test executor assignment (not construction)
    void
    testAssignment()
    {
        asio::io_context ioc;
        capy::executor ex;
        BOOST_TEST(!static_cast<bool>(ex));

        ex = wrap_executor(ioc.get_executor());
        BOOST_TEST(static_cast<bool>(ex));

        bool called = false;
        ex.post([&called]{ called = true; });

        test::run(ioc);
        BOOST_TEST(called);
    }

    // Test multiple assignments
    void
    testReassignment()
    {
        asio::io_context ioc1;
        asio::io_context ioc2;

        capy::executor ex = wrap_executor(ioc1.get_executor());

        bool called1 = false;
        ex.post([&called1]{ called1 = true; });
        test::run(ioc1);
        BOOST_TEST(called1);

        // Reassign to different executor
        ex = wrap_executor(ioc2.get_executor());

        bool called2 = false;
        ex.post([&called2]{ called2 = true; });
        test::run(ioc2);
        BOOST_TEST(called2);
    }

    // Test that the underlying asio executor is valid
    void
    testUnderlyingExecutorValid()
    {
        asio::io_context ioc;
        auto asio_exec = ioc.get_executor();

        // Wrap it
        capy::executor ex = wrap_executor(asio_exec);

        // Post work multiple times to stress test
        int count = 0;
        for(int i = 0; i < 100; ++i)
        {
            ex.post([&count]{ ++count; });
        }

        test::run(ioc);
        BOOST_TEST_EQ(count, 100);
    }

    // Test wrap_executor with temporary executor
    void
    testWrapTemporary()
    {
        asio::io_context ioc;

        // Wrap a temporary - this is how it's used in http_stream
        capy::executor ex = wrap_executor(ioc.get_executor());

        bool called = false;
        ex.post([&called]{ called = true; });

        test::run(ioc);
        BOOST_TEST(called);
    }

    // Test that asio_executor_impl is properly initialized
    void
    testExecutorImplInitialized()
    {
        asio::io_context ioc;

        // Create the wrapper directly to test initialization
        detail::asio_executor_impl<asio::io_context::executor_type> impl(
            ioc.get_executor());

        // The impl should be usable - create work and submit it
        struct test_work : capy::executor::work
        {
            bool& called;
            explicit test_work(bool& c) : called(c) {}
            void invoke() override { called = true; }
        };

        bool called = false;

        // Allocate, construct, and submit work
        void* storage = capy::executor::access::allocate(
            impl, sizeof(test_work), alignof(test_work));
        test_work* w = ::new(storage) test_work(called);
        capy::executor::access::submit(impl, w);

        test::run(ioc);
        BOOST_TEST(called);
    }

    // Test that moved asio_executor_impl is valid
    void
    testExecutorImplAfterMove()
    {
        asio::io_context ioc;

        detail::asio_executor_impl<asio::io_context::executor_type> impl1(
            ioc.get_executor());

        // Move the impl
        auto impl2 = std::move(impl1);

        struct test_work : capy::executor::work
        {
            bool& called;
            explicit test_work(bool& c) : called(c) {}
            void invoke() override { called = true; }
        };

        bool called = false;
        void* storage = capy::executor::access::allocate(
            impl2, sizeof(test_work), alignof(test_work));
        test_work* w = ::new(storage) test_work(called);
        capy::executor::access::submit(impl2, w);

        test::run(ioc);
        BOOST_TEST(called);
    }

    void
    run()
    {
        testWrapExecutor();
        testPostLambda();
        testPostMultiple();
        testPostWithCapture();
        testPostWithMoveOnlyCapture();
        testCopyExecutor();
        testMoveExecutor();
        testWithStrand();
        testAssociatedAllocator();
        testAsyncPostNonVoid();
        testAsyncPostVoid();
        testAsyncPostException();
        testStoredInStruct();
        testStoredInStructAfterMove();
        testAssignment();
        testReassignment();
        testUnderlyingExecutorValid();
        testWrapTemporary();
        testExecutorImplInitialized();
        testExecutorImplAfterMove();
    }
};

TEST_SUITE(
    wrap_executor_test,
    "boost.beast2.wrap_executor");

} // beast2
} // boost

