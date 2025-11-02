//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/application.hpp>
#include <boost/beast2/server/logger.hpp>
#include <boost/beast2/detail/except.hpp>
#include <mutex>
#include <vector>

namespace boost {
namespace beast2 {

enum application::state : char
{
    none,
    starting,
    running,
    stopping,
    stopped
};

struct application::impl
{
    std::mutex m;
    log_sections sections;
    std::vector<std::unique_ptr<part>> v;
    state st = state::none;
    rts::context services;

    ~impl()
    {
        // destroy in reverse order
        while(! v.empty())
            v.resize(v.size() - 1);
    }
};

application::part::~part() = default;

application::
~application()
{
    {
        std::lock_guard<std::mutex> lock(impl_->m);
        if( impl_->st != state::stopped &&
            impl_->st != state::none)
        {
            // stop() hasn't returned yet
            detail::throw_invalid_argument();
        }
    }
    delete impl_;
}

application::
application()
    : impl_(new impl)
{
}

void
application::
start()
{
    {
        std::lock_guard<std::mutex> lock(impl_->m);
        if(impl_->st != state::none)
        {
            // can't call twice
            detail::throw_invalid_argument();
        }
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
    }
    {
        std::lock_guard<std::mutex> lock(impl_->m);
        impl_->st = state::running;
    }
}

void
application::
stop()
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

rts::context&
application::
services() noexcept
{
    return impl_->services;
}

log_sections&
application::
sections() noexcept
{
    return impl_->sections;
}

void
application::
insert(std::unique_ptr<part> pp)
{
    impl_->v.emplace_back(std::move(pp));
}

} // beast2
} // boost
