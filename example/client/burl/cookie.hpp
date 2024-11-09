//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BURL_COOKIES_HPP
#define BURL_COOKIES_HPP

#include <boost/url.hpp>

#include <chrono>
#include <iostream>
#include <list>

namespace core = boost::core;
namespace urls = boost::urls;

struct cookie
{
    enum same_site_t
    {
        strict,
        lax,
        none
    };

    std::string name;
    boost::optional<std::string> value;
    boost::optional<std::chrono::system_clock::time_point> expires;
    boost::optional<std::string> domain;
    boost::optional<std::string> path;
    boost::optional<same_site_t> same_site;
    bool partitioned = false;
    bool secure      = false;
    bool http_only   = false;
};

boost::system::result<cookie>
parse_cookie(core::string_view sv);

class cookie_jar
{
    struct meta_t
    {
        bool subdomains = true;

        meta_t() = default;
        meta_t(bool subdomains_)
            : subdomains{ subdomains_ }
        {
        }
    };

    struct pair_t
    {
        meta_t m;
        cookie c;

        pair_t() = default;
        pair_t(meta_t m_, cookie c_)
            : m{ m_ }
            , c{ c_ }
        {
        }
    };

    std::list<pair_t> cookies_;

public:
    void
    add(const urls::url_view& url, cookie c);

    std::string
    make_field(const urls::url_view& url);

    void
    clear_session_cookies();

    friend
    std::ostream&
    operator<<(std::ostream& os, const cookie_jar& cj);

    friend
    std::istream&
    operator>>(std::istream& is, cookie_jar& cj);
};

#endif
