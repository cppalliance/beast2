//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/server/logger.hpp>
#include <iostream>
#include <mutex>
#include <unordered_map>

namespace boost {
namespace beast2 {

section::
section() noexcept = default;

void
section::
write(
    int level,
    boost::core::string_view s)
{
    (void)level;
    // VFALCO NO! This is just for demo purposes
    static std::mutex m;
    std::lock_guard<std::mutex> lock(m);
    std::cerr << s << std::endl;
}

void
section::
format_impl(
    int level,
    core::string_view fs,
    char const* data,
    std::size_t* plen,
    std::size_t n)
{
    std::string s = impl_->name;
    s.push_back(' ');
    char const* p = fs.data();
    char const* end = fs.data() + fs.size();
    auto p0 = p;
    while(p != end)
    {
        if(*p++ != '{')
            continue;
        if(p == end)
        {
            s.append(p0, p - p0);
            break;
        }
        if(*p++ != '}')
            continue;
        s.append(p0, p - p0 - 2);
        if(n)
        {
            s.append(data, *plen);
            data += *plen++;
            --n;
        }
        p0 = p;
    }
    write(level, s);
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

    std::unordered_map<core::string_view, section, hash> map;
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
    auto it = impl_->map.find(name);
    if(it != impl_->map.end())
        return it->second;
    auto v = section(name);
    impl_->map.emplace( core::string_view(v.impl_->name), v);
    return v;
}

} // beast2
} // boost
