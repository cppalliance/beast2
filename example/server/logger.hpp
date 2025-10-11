//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_EXAMPLE_SERVER_LOG_HPP
#define BOOST_HTTP_IO_EXAMPLE_SERVER_LOG_HPP

#include <boost/core/detail/string_view.hpp>
#include <sstream>

struct section
{
    int threshold() const noexcept { return 0; }

    template<class... Args>
    void format(int level, Args const&... args)
    {
        std::string s;
        {
            std::stringstream ss;
            append(ss, args...);
            s = ss.str();
        }
        write(level, s);
    }

private:
    template<class T1, class T2, class... TN>
    static void append(
        std::ostream& os,
        T1 const& t1,
        T2 const& t2,
        TN const&... tn)
    {
        os << t1;
        append(os, t2, tn...);
    }

    template<class T>
    static void append(
        std::ostream& os,
        T const& t)
    {
        os << t;
    }

    void write(int, boost::core::string_view);
};

#ifndef LOG_AT_LEVEL
#define LOG_AT_LEVEL(sect, level, ...) \
    do { \
        if(level >= sect.threshold()) \
            sect.format(level, __VA_ARGS__); \
    } while(false)
#endif

/// Log at trace level
#ifndef LOG_TRC
#define LOG_TRC(sect, ...) LOG_AT_LEVEL(sect, 0, __VA_ARGS__)
#endif

/// Log at debug level
#ifndef LOG_DBG
#define LOG_DBG(sect, ...) LOG_AT_LEVEL(sect, 1, __VA_ARGS__)
#endif

/// Log at info level (normal)
#ifndef LOG_INF
#define LOG_INF(sect, ...) LOG_AT_LEVEL(sect, 2, __VA_ARGS__)
#endif

/// Log at warning level
#ifndef LOG_WRN
#define LOG_WRN(sect, ...) LOG_AT_LEVEL(sect, 3, __VA_ARGS__)
#endif

/// Log at error level
#ifndef LOG_ERR
#define LOG_ERR(sect, ...) LOG_AT_LEVEL(sect, 4, __VA_ARGS__)
#endif

/// Log at fatal level
#ifndef LOG_FTL
#define LOG_FTL(sect, ...) LOG_AT_LEVEL(sect, 5, __VA_ARGS__)
#endif

#endif
