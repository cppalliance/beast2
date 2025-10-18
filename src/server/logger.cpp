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

void
section::
write(
    int level,
    boost::core::string_view s)
{
    (void)level;
    static std::mutex m;
    std::lock_guard<std::mutex> lock(m);
    std::cerr << s << std::endl;
}
