//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include "cookie.hpp"

#include <boost/url/grammar.hpp>
#include <boost/url/grammar/all_chars.hpp>

#include <iomanip>
#include <sstream>

#ifdef BURL_HAS_LIBPSL
#include <libpsl.h>
#endif

namespace grammar = boost::urls::grammar;

namespace
{
struct name_chars_t
{
    constexpr bool
    operator()(char c) const noexcept
    {
        // clang-format off
        return
            c > 0x20 && c != 0x7F &&
            c != '(' && c != ')'  && c != '<' && c != '>'  && c != '@' &&
            c != ',' && c != ';'  && c != ':' && c != '\\' && c != '"' &&
            c != '/' && c != '['  && c != ']' && c != '?'  && c != '=' &&
            c != '{' && c != '}';
        // clang-format on
    }
};

constexpr auto name_chars = name_chars_t{};

struct value_chars_t
{
    constexpr bool
    operator()(char c) const noexcept
    {
        // clang-format off
        return
            (c == 0x21             ) ||
            (c >= 0x23 && c <= 0x2B) ||
            (c >= 0x2D && c <= 0x3A) ||
            (c >= 0x3C && c <= 0x5B) ||
            (c >= 0x5D && c <= 0x7E);
        // clang-format on
    }
};

constexpr auto value_chars = value_chars_t{};

constexpr auto attr_chars =
    urls::grammar::all_chars - urls::grammar::lut_chars("\x1F\x7f;");

bool
domain_match(
    core::string_view r_domain,
    core::string_view c_domain,
    bool tailmatch) noexcept
{
    if(!tailmatch)
        return r_domain == c_domain;

    if(c_domain.starts_with('.'))
        c_domain.remove_prefix(1);

    if(r_domain.ends_with(c_domain))
    {
        if(r_domain.size() == c_domain.size())
            return true;

        return r_domain[r_domain.size() - c_domain.size() - 1] == '.';
    }

    return false;
}

bool
path_match(core::string_view r_path, core::string_view c_path) noexcept
{
    if(r_path.empty())
        return true;

    if(r_path.starts_with(c_path))
    {
        if(r_path.size() == c_path.size())
            return true;

        if(c_path.ends_with('/'))
            return true;

        return r_path[r_path.size() - c_path.size()] == '/';
    }

    return false;
}

ch::system_clock::time_point
parse_date(core::string_view sv)
{
    // TODO: There are more date formats; we need a
    // better parsing method.
    auto tm = std::tm{};
    auto ss = std::stringstream{ sv };

    ss >> std::get_time(
              &tm,
              sv.contains('-') ? "%a, %d-%b-%Y %H:%M:%S GMT"
                               : "%a, %d %b %Y %H:%M:%S GMT");

    return ch::system_clock::from_time_t(std::mktime(&tm));
}

boost::system::result<cookie>
parse_netscape_cookie(core::string_view sv)
{
    static constexpr auto field_chars =
        grammar::all_chars - grammar::lut_chars{ "\t" };

    static constexpr auto netscape_parser = grammar::tuple_rule(
        grammar::optional_rule(grammar::literal_rule("#HttpOnly_")),
        grammar::token_rule(field_chars),
        grammar::squelch(grammar::delim_rule('\t')),
        grammar::variant_rule(
            grammar::literal_rule("FALSE"), grammar::literal_rule("TRUE")),
        grammar::squelch(grammar::delim_rule('\t')),
        grammar::token_rule(field_chars),
        grammar::squelch(grammar::delim_rule('\t')),
        grammar::variant_rule(
            grammar::literal_rule("FALSE"), grammar::literal_rule("TRUE")),
        grammar::squelch(grammar::delim_rule('\t')),
        grammar::unsigned_rule<std::uint32_t>(),
        grammar::squelch(grammar::delim_rule('\t')),
        grammar::token_rule(field_chars),
        grammar::squelch(grammar::delim_rule('\t')),
        grammar::token_rule(field_chars));

    const auto parse_rs = grammar::parse(sv, netscape_parser);

    if(parse_rs.has_error())
        return parse_rs.error();

    auto epoch_to_expiry = [](std::uint32_t epoch)
        -> boost::optional<std::chrono::system_clock::time_point>
    {
        if(epoch == 0)
            return boost::none;

        return ch::system_clock::from_time_t(static_cast<std::time_t>(epoch));
    };

    auto rs      = cookie{};
    rs.http_only = std::get<0>(*parse_rs).has_value();
    rs.domain    = std::get<1>(*parse_rs);
    rs.tailmatch = std::get<2>(*parse_rs).index();
    rs.path      = std::get<3>(*parse_rs);
    rs.secure    = std::get<4>(*parse_rs).index();
    rs.expires   = epoch_to_expiry(std::get<5>(*parse_rs));
    rs.name      = std::get<6>(*parse_rs);
    rs.value     = std::get<7>(*parse_rs);
    return rs;
}
} // namespace

boost::system::result<cookie>
parse_cookie(core::string_view sv)
{
    static constexpr auto cookie_parser = grammar::tuple_rule(
        grammar::token_rule(name_chars),
        grammar::squelch(grammar::delim_rule('=')),
        grammar::optional_rule(grammar::token_rule(value_chars)),
        grammar::range_rule(
            grammar::tuple_rule(
                grammar::squelch(grammar::delim_rule(';')),
                grammar::squelch(
                    grammar::optional_rule(grammar::delim_rule(' '))),
                grammar::token_rule(attr_chars - grammar::lut_chars('=')),
                grammar::squelch(
                    grammar::optional_rule(grammar::delim_rule('='))),
                grammar::optional_rule(grammar::token_rule(attr_chars)))));

    const auto parse_rs = grammar::parse(sv, cookie_parser);

    if(parse_rs.has_error())
        return parse_rs.error();

    auto rs  = cookie{};
    rs.name  = std::get<0>(parse_rs.value());
    rs.value = std::get<1>(parse_rs.value());

    for(auto&& attr : std::get<2>(parse_rs.value()))
    {
        auto name  = std::get<0>(attr);
        auto value = std::get<1>(attr);

        if(grammar::ci_is_equal(name, "Expires"))
        {
            if(!value)
                return grammar::error::invalid;

            rs.expires = parse_date(*value);
        }
        else if(grammar::ci_is_equal(name, "Max-Age"))
        {
            if(!value)
                return grammar::error::invalid;
            // Convert to expiry date
            // TODO: replace std::stoll
            // TODO: check for overflow
            rs.expires =
                ch::system_clock::now() + ch::seconds{ std::stoll(*value) };
        }
        else if(grammar::ci_is_equal(name, "Domain"))
        {
            if(!value)
                return grammar::error::invalid;

            rs.domain = *value;
        }
        else if(grammar::ci_is_equal(name, "Path"))
        {
            if(!value)
                return grammar::error::invalid;
            rs.path = *value;
        }
        else if(grammar::ci_is_equal(name, "SameSite"))
        {
            if(grammar::ci_is_equal(value.value_or(""), "Strict"))
                rs.same_site = cookie::same_site_t::strict;
            else if(grammar::ci_is_equal(value.value_or(""), "Lax"))
                rs.same_site = cookie::same_site_t::lax;
            else if(grammar::ci_is_equal(value.value_or(""), "None"))
                rs.same_site = cookie::same_site_t::none;
            else
                return grammar::error::invalid;
        }
        else if(grammar::ci_is_equal(name, "Partitioned"))
        {
            rs.partitioned = true;
        }
        else if(grammar::ci_is_equal(name, "Secure"))
        {
            rs.secure = true;
        }
        else if(grammar::ci_is_equal(name, "HttpOnly"))
        {
            rs.http_only = true;
        }
    }

    // "__Secure-" prefix requirements
    if(core::string_view{ rs.name }.starts_with("__Secure-"))
    {
        if(!rs.secure)
            return grammar::error::invalid;
    }

    // "__Host-" prefix requirements
    if(core::string_view{ rs.name }.starts_with("__Host-"))
    {
        if(!rs.secure)
            return grammar::error::invalid;

        if(!rs.path || rs.path.value() != "/")
            return grammar::error::invalid;

        if(rs.domain.has_value())
            return grammar::error::invalid;
    }

    return rs;
}

void
cookie_jar::add(const urls::url_view& url, cookie c)
{
    if(c.domain.has_value())
    {
#ifdef BURL_HAS_LIBPSL
        std::string host = url.host();
        for(auto& c : host)
            c = grammar::to_lower(c);

        std::string domain = c.domain.value();
        for(auto& c : domain)
            c = grammar::to_lower(c);

        if(psl_is_cookie_domain_acceptable(
            psl_builtin(), host.c_str(), domain.c_str()))
            return;
#endif
        c.tailmatch = true;
    }
    else
    {
        c.domain.emplace(url.host());
    }

    if(!c.path.has_value())
    {
        c.path.emplace();
        auto segs = url.encoded_segments();
        auto end  = std::prev(segs.end(), !segs.empty());
        for(auto it = segs.begin(); it != end; ++it)
        {
            c.path->push_back('/');
            c.path->append(it->begin(), it->end());
        }
        if(c.path->empty())
            c.path->push_back('/');
    }

    if(c.secure && url.scheme_id() != urls::scheme::https)
        return;

    cookies_.erase(
        std::remove_if(
            cookies_.begin(),
            cookies_.end(),
            [&](const cookie& o)
            {
                return c.name == o.name && c.path == o.path &&
                    c.domain == o.domain;
            }),
        cookies_.end());

    // Check expiry date last to allow servers to remove cookies
    if(c.expires.has_value() && c.expires.value() < ch::system_clock::now())
        return;

    cookies_.push_back(std::move(c));
}

std::string
cookie_jar::make_field(const urls::url_view& url)
{
    const auto r_domain    = url.host();
    const auto r_path      = url.encoded_path();
    const auto r_is_secure = url.scheme_id() == urls::scheme::https;
    const auto now         = ch::system_clock::now();

    auto rs = std::string{};
    for(auto it = cookies_.begin(); it != cookies_.end();)
    {
        if(it->expires.has_value() && it->expires <= now)
        {
            it = cookies_.erase(it);
            continue;
        }

        if(domain_match(r_domain, it->domain.value(), it->tailmatch) &&
           path_match(r_path, it->path.value()) &&
           (it->secure ? r_is_secure : true))
        {
            rs.append(it->name);
            rs.push_back('=');
            if(it->value.has_value())
                rs.append(*it->value);
            rs.append("; ");
        }

        ++it;
    }
    return rs;
}

void
cookie_jar::clear_session_cookies()
{
    cookies_.erase(
        std::remove_if(
            cookies_.begin(),
            cookies_.end(),
            [](const cookie& c) { return !c.expires.has_value(); }),
        cookies_.end());
}

std::ostream&
operator<<(std::ostream& os, const cookie_jar& cj)
{
    os << "# Netscape HTTP Cookie File\n\n";

    for(const auto& c : cj.cookies_)
    {
        os << (c.http_only ? "#HttpOnly_" : "");
        os << c.domain.value() << '\t';
        os << (c.tailmatch ? "TRUE" : "FALSE") << '\t';
        os << c.path.value() << '\t';
        os << (c.secure ? "TRUE" : "FALSE") << '\t';
        if(c.expires)
            os << ch::duration_cast<ch::seconds>(
                      c.expires.value().time_since_epoch())
                      .count();
        else
            os << '0';
        os << '\t';
        os << c.name << '\t';
        os << c.value.value_or("");
        os << '\n';
    }
    return os;
}

std::istream&
operator>>(std::istream& is, cookie_jar& cj)
{
    for(std::string line; getline(is, line);)
    {
        if(line.empty())
            continue;

        // skip comments
        if(line.starts_with('#') && !line.starts_with("#HttpOnly_"))
            continue;

        cj.cookies_.push_back(parse_netscape_cookie(line).value());
    }
    return is;
}
