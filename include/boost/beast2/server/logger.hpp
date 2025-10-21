//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_LOGGER_HPP
#define BOOST_BEAST2_SERVER_LOGGER_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/core/detail/string_view.hpp>
#include <memory>
#include <sstream>
#include <string>

namespace boost {
namespace beast2 {

struct section
{
    BOOST_BEAST2_DECL
    section() noexcept;

    /** Return the level below which logging is squelched
    */
    int threshold() const noexcept
    {
        return impl_->level;
    }

    template<class... Args>
    void operator()(
        core::string_view const& fs,
        Args const&... args)
    {
        int level = 0;

        auto const N = sizeof...(Args);
        std::size_t len[N];
        std::stringstream ss;
        write(ss, len, args...);
        // VFALCO This makes an unnecessary copy
        std::string s(ss.str());
        format_impl(level, fs, s.data(), len, N);
    }

private:
    template<class T1, class T2, class... TN>
    static void write(
        std::stringstream& ss,
        std::size_t* plen,
        T1 const& t1,
        T2 const& t2,
        TN const&... tn)
    {
        auto const n0 = ss.rdbuf()->pubseekoff(
            0, std::ios::cur, std::ios::out);
        ss << t1;
        auto const n1 = ss.rdbuf()->pubseekoff(
            0, std::ios::cur, std::ios::out);
        *plen = n1 - n0;
        write(ss, ++plen, t2, tn...);
    }

    template<class T>
    static void write(
        std::stringstream& ss,
        std::size_t* plen,
        T const& t)
    {
        auto const n0 = ss.str().size();
        ss << t;
        *plen = ss.str().size() - n0;
    }

    BOOST_BEAST2_DECL
    void format_impl(int, core::string_view,
        char const*, std::size_t*, std::size_t n);

    BOOST_BEAST2_DECL
    void write(int, boost::core::string_view);

    section(core::string_view);

    friend class log_sections;

    struct impl
    {
        std::string name;
        int level = 0;
    };

    std::shared_ptr<impl> impl_;
};

//------------------------------------------------

#if 0
class log_stream
{
public:
    log_stream() = default;

    log_stream(section& sect, int level)
        : sect_(&sect)
        , level_(level)
    {
    }

    template<class... Args>
    void operator()(Args const&... args)
    {
        if( sect_)
            sect_->operator()(args...);
    }

private:
    section* sect_ = nullptr;
    int level_ = 0;
};
#endif

//------------------------------------------------

class log_sections
{
public:
    /** Destructor
    */
    BOOST_BEAST2_DECL
    ~log_sections();

    /** Constructor
    */
    BOOST_BEAST2_DECL
    log_sections();

    /** Return a log section by name.

        If the section does not already exist, it is created.
        The name is case sensitive.
    */
    BOOST_BEAST2_DECL
    section
    get(core::string_view name);

private:
    struct impl;
    impl* impl_;
};

//------------------------------------------------

#ifndef LOG_AT_LEVEL
#define LOG_AT_LEVEL(sect, level) \
    if((level) < (sect).threshold()) {} else sect
#endif

/// Log at trace level
#ifndef LOG_TRC
#define LOG_TRC(sect) LOG_AT_LEVEL(sect, 0)
#endif

/// Log at debug level
#ifndef LOG_DBG
#define LOG_DBG(sect) LOG_AT_LEVEL(sect, 1)
#endif

/// Log at info level (normal)
#ifndef LOG_INF
#define LOG_INF(sect) LOG_AT_LEVEL(sect, 2)
#endif

/// Log at warning level
#ifndef LOG_WRN
#define LOG_WRN(sect) LOG_AT_LEVEL(sect, 3)
#endif

/// Log at error level
#ifndef LOG_ERR
#define LOG_ERR(sect) LOG_AT_LEVEL(sect, 4)
#endif

/// Log at fatal level
#ifndef LOG_FTL
#define LOG_FTL(sect, ...) LOG_AT_LEVEL(sect, 5, __VA_ARGS__)
#endif

} // beast2
} // boost

#endif
