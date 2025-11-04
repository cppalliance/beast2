//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/polystore.hpp>
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

    std::vector<ptr> v;
    std::unordered_map<
        detail::type_info const*, any*, hash, eq> m;

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
    return it->second->get();
}

void
polystore::
insert(
    detail::type_info const* key, ptr p)
{
    // ensure emplace_back can't fail
    impl_->v.reserve(impl_->v.size() + 1);
    if(key)
    {
        auto result = impl_->m.emplace(key, p.get());
        if(! result.second)
            detail::throw_invalid_argument();
    }
    impl_->v.emplace_back(std::move(p));
}

} // beast2
} // boost
