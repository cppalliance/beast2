//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include <boost/http_io/server/logger.hpp>
#include <iostream>
#include <mutex>

namespace boost {
namespace http_io {

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
    std::string s;
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

} // http_io
} // boost
