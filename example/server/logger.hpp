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

#include "format.hpp"
#include <boost/core/detail/string_view.hpp>
#include <sstream>

struct section
{
    int threshold() const noexcept { return 0; }

#if defined(BOOST_HTTP_IO_HAS_STD_FORMAT) || defined(BOOST_HTTP_IO_HAS_FMT)
    // Format string version when std::format or fmtlib is available
    template<class... Args>
    void format(int level, std::string_view fmt, Args const&... args)
    {
        write(level, detail::format(fmt, args...));
    }
#else
    // Legacy variadic template version for fallback
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

public:
#endif

    void write(int, boost::core::string_view);
};

#define LOG_AT_LEVEL(sect, level, ...) \
    do { \
        if(level >= sect.threshold()) \
            sect.format(level, __VA_ARGS__); \
    } while(false)

/// Log at trace level
#define LOG_TRC(sect, ...) LOG_AT_LEVEL(sect, 0, __VA_ARGS__)

/// Log at debug level
#define LOG_DBG(sect, ...) LOG_AT_LEVEL(sect, 1, __VA_ARGS__)

/// Log at info level (normal)
#define LOG_INF(sect, ...) LOG_AT_LEVEL(sect, 2, __VA_ARGS__)

/// Log at warning level
#define LOG_WRN(sect, ...) LOG_AT_LEVEL(sect, 3, __VA_ARGS__)

/// Log at error level
#define LOG_ERR(sect, ...) LOG_AT_LEVEL(sect, 4, __VA_ARGS__)

/// Log at fatal level
#define LOG_FTL(sect, ...) LOG_AT_LEVEL(sect, 5, __VA_ARGS__)

#endif
