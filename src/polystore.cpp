//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/polystore.hpp>
#include <boost/assert.hpp>
#include <unordered_map>
#include <vector>

namespace boost {
namespace beast2 {

struct polystore::impl
{
    struct hash
    {
        std::size_t operator()(
            detail::type_info const* p) const noexcept
        {
            return p->hash_code();
        }
    };

    struct eq
    {
        bool operator()(
            detail::type_info const* p0,
            detail::type_info const* p1) const noexcept
        {
            return *p0 == *p1;
        }
    };

    std::vector<any_ptr> v;
    std::unordered_map<
        detail::type_info const*, void*, hash, eq> m;

    ~impl()
    {
        // destroy in reverse order
        while(! v.empty())
            v.resize(v.size() - 1);
    }
};

polystore::any::~any() = default;

polystore::
~polystore()
{
    delete impl_;
}

polystore::
polystore()
    : impl_(new impl)
{
}

auto
polystore::
get_elements() noexcept ->
    elements
{
    return elements(impl_->v.size(), *this);
}

auto
polystore::
get(std::size_t i) -> any&
{
    return *impl_->v[i];
}

void*
polystore::
find(
    detail::type_info const& ti) const noexcept
{
    auto const it = impl_->m.find(&ti);
    if(it == impl_->m.end())
        return nullptr;
    return it->second;
}

void*
polystore::
insert_impl(
    any_ptr p, key const* k, std::size_t n)
{
    struct do_insert
    {
        any_ptr p;
        key const* k;
        std::size_t n;
        polystore::impl* pi;
        std::size_t i = 0;

        do_insert(
            any_ptr p_,
            key const* k_,
            std::size_t n_,
            polystore::impl* pi_)
            : p(std::move(p_)), k(k_), n(n_), pi(pi_)
        {
            // ensure emplace_back can't fail
            pi->v.reserve(pi->v.size() + 1);

            for(;i < n;++i)
                if(! pi->m.emplace(k[i].ti, k[i].p).second)
                    detail::throw_invalid_argument(
                        "polystore: duplicate key");

            pi->v.emplace_back(std::move(p));
        }

        ~do_insert()
        {
            if(i == n)
                return;
            while(i--)
                pi->m.erase(k[i].ti);
        }
    };

    auto const pt = p->get();
    do_insert(std::move(p), k, n, impl_);
    return pt;
}

} // beast2
} // boost
