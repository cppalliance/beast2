//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_FORMAT_HPP
#define BOOST_BEAST2_FORMAT_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/core/detail/string_view.hpp>
#include <memory>
#include <ostream>
#include <streambuf>
#include <string>

namespace boost {
namespace beast2 {

namespace detail {

struct format_impl
{
    std::ostream& os;
    core::string_view fs;
    char const* p;
    char const* p0;
    char const* end;
    bool has_placeholder = false;

    format_impl(
        std::ostream& os_,
        core::string_view fs_)
        : os(os_)
        , fs(fs_)
        , p(fs.data())
        , p0(p)
        , end(p + fs.size())
    {
    }

    core::string_view
    next()
    {
        has_placeholder = false;
        while (p != end)
        {
            if (*p++ != '{')
                continue;
            if (p == end)
                break;
            if (*p++ == '}')
            {
                core::string_view seg(
                    p0, (p - 2) - p0);
                p0 = p;
                has_placeholder = true;
                return seg;
            }
        }
        core::string_view seg(
            p0, end - p0);
        p0 = end;
        return seg;
    }

    template<class Arg>
    void do_arg(Arg const& arg)
    {
        core::string_view seg = next();
        if (seg.size())
            os.write(seg.data(), static_cast<
                std::streamsize>(seg.size()));
        if (has_placeholder)
            os << arg;
    };

    template<class... Args>
    void operator()(Args const&... args)
    {
        using expander = int[];
        (void)expander{0, (do_arg(args), 0)...};
        if (p0 < end)
            os.write(p0, static_cast<
                std::streamsize>(end - p0));
    }
};

class appendbuf : public std::streambuf
{
    std::string* s_;

protected:
    // Called when a single character is to be written
    virtual int_type overflow(int_type ch) override
    {
        if (ch != traits_type::eof())
        {
            s_->push_back(static_cast<char>(ch));
            return ch;
        }
        return traits_type::eof();
    }

    // Called when multiple characters are to be written
    virtual std::streamsize xsputn(const char* s, std::streamsize n) override
    {
        s_->append(s, static_cast<std::size_t>(n));
        return n;
    }

public:
    explicit appendbuf(std::string& s)
        : s_(&s)
    {
    }
};

class appendstream : public std::ostream
{
    appendbuf buf_;

public:
    explicit appendstream(std::string& s)
        : std::ostream(&buf_)
        , buf_(s)
    {
    }
};

} // detail

/** Format arguments using a format string
*/
template<class... Args>
void
format_to(
    std::ostream& os,
    core::string_view fs,
    Args const&... args)
{
    detail::format_impl(os, fs)(args...);
}

/** Format arguments using a format string
*/
template<class... Args>
void
format_to(
    std::string& dest,
    core::string_view fs,
    Args const&... args)
{
    detail::appendstream ss(dest);
    format_to(ss, fs, args...);
}

} // beast2
} // boost

#endif
