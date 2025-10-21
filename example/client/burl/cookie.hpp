//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BURL_COOKIES_HPP
#define BURL_COOKIES_HPP

#include <boost/optional.hpp>
#include <boost/url/url_view.hpp>

#include <chrono>
#include <iostream>
#include <list>

namespace ch   = std::chrono;
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
    boost::optional<ch::system_clock::time_point> expires;
    boost::optional<std::string> domain;
    boost::optional<std::string> path;
    boost::optional<same_site_t> same_site;
    bool partitioned = false;
    bool secure      = false;
    bool http_only   = false;
    bool tailmatch   = false;
};

boost::system::result<cookie>
parse_cookie(core::string_view sv);

class cookie_jar
{
    std::list<cookie> cookies_;

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
