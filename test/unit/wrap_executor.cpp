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
#include <boost/capy/any_dispatcher.hpp>
#include <boost/capy/async_run.hpp>
#include <boost/capy/task.hpp>

namespace boost {
namespace beast2 {

//-----------------------------------------------------------------------------

struct wrap_executor_test
{
    void
    testWrapExecutor()
    {
        // Verify wrap_executor returns a type that satisfies dispatcher concept
        asio::io_context ioc;
        auto dispatcher = wrap_executor(ioc.get_executor());
        
        // Should be storable in any_dispatcher
        capy::any_dispatcher any_disp(dispatcher);
        BOOST_TEST(static_cast<bool>(any_disp));
    }

    void
    testDispatchCoroutine()
    {
        asio::io_context ioc;
        
        bool resumed = false;
        
        // Create and run a simple coroutine
        auto make_task = [&resumed]() -> capy::task<void> {
            resumed = true;
            co_return;
        };
        
        // Launch using async_run with our dispatcher
        capy::async_run(wrap_executor(ioc.get_executor()))(make_task());
        
        // Coroutine should not have run yet (just scheduled)
        BOOST_TEST(!resumed);
        
        // Run the io_context to execute posted work
        test::run(ioc);
        
        // Now it should have run
        BOOST_TEST(resumed);
    }

    void
    testWithStrand()
    {
        asio::io_context ioc;
        asio::strand<asio::io_context::executor_type> strand(
            ioc.get_executor());
        auto dispatcher = wrap_executor(strand);
        
        // Should be storable in any_dispatcher
        capy::any_dispatcher any_disp(dispatcher);
        BOOST_TEST(static_cast<bool>(any_disp));
    }

    void
    testAnyDispatcherStorage()
    {
        // Test pattern used in http route_params
        struct params
        {
            capy::any_dispatcher ex;
        };

        asio::io_context ioc;
        params p;
        p.ex = wrap_executor(ioc.get_executor());
        
        BOOST_TEST(static_cast<bool>(p.ex));
    }

    void
    testAnyDispatcherCopy()
    {
        asio::io_context ioc;
        auto dispatcher = wrap_executor(ioc.get_executor());
        
        capy::any_dispatcher disp1(dispatcher);
        capy::any_dispatcher disp2 = disp1;
        
        BOOST_TEST(static_cast<bool>(disp1));
        BOOST_TEST(static_cast<bool>(disp2));
        BOOST_TEST(disp1 == disp2);
    }

    void
    testMultipleCoroutines()
    {
        asio::io_context ioc;
        
        int count = 0;
        
        auto make_task = [&count]() -> capy::task<void> {
            ++count;
            co_return;
        };
        
        // Launch multiple coroutines
        capy::async_run(wrap_executor(ioc.get_executor()))(make_task());
        capy::async_run(wrap_executor(ioc.get_executor()))(make_task());
        capy::async_run(wrap_executor(ioc.get_executor()))(make_task());
        
        BOOST_TEST_EQ(count, 0);
        test::run(ioc);
        BOOST_TEST_EQ(count, 3);
    }

    void
    run()
    {
        testWrapExecutor();
        testDispatchCoroutine();
        testWithStrand();
        testAnyDispatcherStorage();
        testAnyDispatcherCopy();
        testMultipleCoroutines();
    }
};

TEST_SUITE(
    wrap_executor_test,
    "boost.beast2.wrap_executor");

} // beast2
} // boost
