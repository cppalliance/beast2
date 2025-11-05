//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/logger.hpp>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace boost {
namespace beast2 {

section::
section() noexcept = default;

void
section::
write(
    int level,
    std::string s)
{
    (void)level;
    // VFALCO NO! This is just for demo purposes
    static std::mutex m;
    std::lock_guard<std::mutex> lock(m);
    std::cerr << s << std::endl;
}

section::
section(core::string_view name)
    : impl_(std::make_shared<impl>())
{
    impl_->name = name;
}

//------------------------------------------------

struct log_sections::impl
{
    struct hash
    {
        std::size_t
        operator()(core::string_view const& s) const noexcept
        {
        #if SIZE_MAX == 4294967295U
            std::size_t hash = 2166136261; // FNV offset basis
            for (unsigned char c : s)
                hash ^= c, hash *= 16777619;   // FNV prime
        #else
            std::size_t hash = 1469598103934665603; // FNV offset basis
            for (unsigned char c : s)
                hash ^= c, hash *= 1099511628211;   // FNV prime
        #endif
            return hash;
        }
    };

    std::mutex m;
    std::unordered_map<core::string_view, section, hash> map;
    std::vector<section> vec;
};

log_sections::
~log_sections()
{
    delete impl_;
}

log_sections::
log_sections()
    : impl_(new impl)
{
}

section
log_sections::
get(core::string_view name)
{
    // contention for this lock should be minimal,
    // as most sections are created at startup.
    std::lock_guard<std::mutex> lock(impl_->m);
    auto it = impl_->map.find(name);
    if(it != impl_->map.end())
        return it->second;
    // the map stores a string_view; make sure
    // the string data it references does not
    // move after creation.
    auto v = section(name);
    impl_->map.emplace(
        core::string_view(v.impl_->name), v);
    impl_->vec.push_back(v);
    return v;
}

auto
log_sections::
get_sections() const noexcept ->
    std::vector<section>
{
    std::lock_guard<std::mutex> lock(impl_->m);
    return impl_->vec;
}

} // beast2
} // boost
