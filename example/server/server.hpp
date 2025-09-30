//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_EXAMPLE_SERVER_HPP
#define BOOST_HTTP_IO_EXAMPLE_SERVER_HPP

#include <boost/rts/context.hpp>
#include <atomic>
#include <memory>
#include <type_traits>
#include <vector>

/** A generic server interface

    In this model a server contains zero or more parts, where each part
    undergoes two-phase initialization. First the part is constructed by
    calling @ref make_part. The @ref run function invokes `run()` on each
    part. When @ref stop is called, each part has its `stop()` member
    invoked. And when the server object is destroyed, all the parts are
    destroyed in reverse order of construction.

    The server also contains an `rts::services` collection VFALCO TODO
*/
class server
{
public:
    class part
    {
    public:
        virtual ~part();
        virtual void run() = 0;
        virtual void stop() = 0;
    };

    server();
    server(server const&) = delete;
    server& operator=(server const&) = delete;

    bool is_stopping() const noexcept
    {
        return is_stopping_;
    }

    /** Return a unique ID

        Workers are identified in log output by ID.
    */
    std::size_t
    make_unique_id() noexcept
    {
        return ++id_;
    }

    boost::rts::context&
    services()
    {
        return services_;
    }

    /** Construct a new server part
    */
    template<class Part, class... Args>
    friend Part& new_part(server&, Args&&... args);

protected:
    boost::rts::context services_;
    std::atomic<std::size_t> id_{0};
    std::vector<std::unique_ptr<part>> v_;
    bool is_stopping_ = false;
    bool is_stopped_ = false;
};

template<class Part, class... Args>
Part&
new_part(
    server& srv,
    Args&&... args)
{
    static_assert(
        std::is_convertible<Part*, server::part*>::value,
        "Type requirements not met.");

    auto p = std::unique_ptr<Part>(new Part(
        srv, std::forward<Args>(args)...));
    auto& part = *p;
    srv.v_.emplace_back(std::move(p));
    return part;
}

//------------------------------------------------

struct resumer
{
    struct impl
    {
        ~impl() = default;
        virtual void resume() = 0;
        virtual void close() = 0;
        virtual void fail() = 0;
    };

    /** Destructor

        If no other members have been invoked, destruction
        of the resumer object is equivalent to calling close().
    */
    ~resumer();

    resumer(std::shared_ptr<impl> sp)
        : sp_(std::move(sp))
    {
    }

    void resume()
    {
        sp_->resume();
    }

    void close()
    {
        sp_->close();
    }

    void fail()
    {
        sp_->fail();
    }

private:
    std::shared_ptr<impl> sp_;
};

struct actions
{
    ~actions() = default;
    virtual void detach() = 0;
    virtual void close() = 0;
    virtual void fail() = 0;
};

#endif
