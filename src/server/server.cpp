//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include <boost/http_io/server/server.hpp>
#include <boost/http_io/detail/except.hpp>
#include <mutex>
#include <vector>

namespace boost {
namespace http_io {

enum server::state : char
{
    none,
    starting,
    running,
    stopping,
    stopped
};

struct server::impl
{
    std::mutex m;
    rts::context services;
    std::vector<std::unique_ptr<part>> v;
    state st = state::none;
};

server::part::~part() = default;

server::
~server()
{
    delete impl_;
}

server::
server()
    : impl_(new impl)
{
}

bool
server::
is_stopping() const noexcept
{
    return
        impl_->st == state::stopping ||
        impl_->st == state::stopped;
}

rts::context&
server::
services() noexcept
{
    return impl_->services;
}

void
server::
install(std::unique_ptr<part> pp)
{
    impl_->v.emplace_back(std::move(pp));
}

void
server::
do_start()
{
    {
        std::lock_guard<std::mutex> lock(impl_->m);
        if(impl_->st != state::none)
            detail::throw_invalid_argument();
        impl_->st = state::starting;
    }
    for(auto it = impl_->v.begin(); it != impl_->v.end(); ++it)
    {
        try
        {
            (*it)->start();
        }
        catch(std::exception const&)
        {
            {
                std::lock_guard<std::mutex> lock(impl_->m);
                impl_->st = state::stopping;
            }
            do
            {
                (*it)->stop();
            }
            while(it-- != impl_->v.begin());
            {
                std::lock_guard<std::mutex> lock(impl_->m);
                impl_->st = state::stopped;
            }
            throw;
        }
        {
            std::lock_guard<std::mutex> lock(impl_->m);
            impl_->st = state::running;
        }
    }
}

void
server::
do_stop()
{
    {
        std::lock_guard<std::mutex> lock(impl_->m);
        if(impl_->st != state::running)
            detail::throw_invalid_argument();
        impl_->st = state::stopping;
    }
    for(auto it = impl_->v.rbegin(); it != impl_->v.rend(); ++it)
        (*it)->stop();
    {
        std::lock_guard<std::mutex> lock(impl_->m);
        impl_->st = state::stopped;
    }
}

} // http_io
} // boost
