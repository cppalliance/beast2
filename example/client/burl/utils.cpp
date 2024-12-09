//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include "utils.hpp"

#include <boost/url.hpp>

namespace grammar  = boost::urls::grammar;
namespace variant2 = boost::variant2;

namespace
{
struct attr_char_t
{
    constexpr bool
    operator()(char c) const noexcept
    {
        return grammar::alnum_chars(c) ||
            c == '!' || c == '#' || c == '$' || c == '&' || c == '+' ||
            c == '-' || c == '.' || c == '^' || c == '_' || c == '`' ||
            c == '|' || c == '~';
    }
};

constexpr attr_char_t attr_char{};

struct value_char_t
{
    constexpr bool
    operator()(char c) const noexcept
    {
        return attr_char(c) || c == '%';
    }
};

constexpr auto value_char = value_char_t{};

struct quoted_string_t
{
    using value_type = core::string_view;

    constexpr boost::system::result<value_type>
    parse(char const*& it, char const* end) const noexcept
    {
        const auto it0 = it;

        if(it == end)
            return grammar::error::need_more;

        if(*it++ != '"')
            return grammar::error::mismatch;

        for(; it != end && *it != '"'; it++)
        {
            if(*it == '\\')
                it++;
        }

        if(*it != '"')
            return grammar::error::mismatch;

        return value_type(it0, ++it);
    }
};

constexpr auto quoted_string = quoted_string_t{};

std::string
unquote_string(core::string_view sv)
{
    sv.remove_prefix(1);
    sv.remove_suffix(1);

    auto rs = std::string{};
    for(auto it = sv.begin(); it != sv.end(); it++)
    {
        if(*it == '\\')
            it++;
        rs.push_back(*it);
    }
    return rs;
}
}

boost::optional<std::string>
extract_filename_form_content_disposition(core::string_view sv)
{
    static constexpr auto parser = grammar::range_rule(
        grammar::tuple_rule(
            grammar::squelch(grammar::optional_rule(grammar::delim_rule(';'))),
            grammar::squelch(grammar::optional_rule(grammar::delim_rule(' '))),
            grammar::token_rule(attr_char),
            grammar::squelch(grammar::optional_rule(grammar::delim_rule('='))),
            grammar::optional_rule(
                grammar::variant_rule(
                    quoted_string, grammar::token_rule(value_char)))));

    const auto parse_rs = grammar::parse(sv, parser);

    if(parse_rs.has_error())
        return boost::none;

    for(auto&& [name, value] : parse_rs.value())
    {
        if(name == "filename" && value.has_value())
        {
            if(value.value().index() == 0)
            {
                return unquote_string(variant2::get<0>(value.value()));
            }
            else
            {
                return std::string{ variant2::get<1>(value.value()) };
            }
        }
    }

    return boost::none;
}

form_option_result
parse_form_option(core::string_view sv)
{
    constexpr auto name_rule =
        grammar::token_rule(grammar::all_chars - grammar::lut_chars('='));

    constexpr auto value_rule = grammar::variant_rule(
        quoted_string,
        grammar::token_rule(grammar::all_chars - grammar::lut_chars(';')));

    static constexpr auto parser = grammar::tuple_rule(
        name_rule,
        grammar::squelch(grammar::delim_rule('=')),
        grammar::optional_rule(grammar::delim_rule(grammar::lut_chars("@<"))),
        value_rule,
        grammar::range_rule(
            grammar::tuple_rule(
                grammar::squelch(grammar::delim_rule(';')),
                name_rule,
                grammar::squelch(grammar::delim_rule('=')),
                value_rule)));

    const auto parse_rs = grammar::parse(sv, parser);

    if(parse_rs.has_error())
        throw std::runtime_error{ "Illegally formatted input field" };

    auto [name, prefix, value, range] = parse_rs.value();

    auto extract = [](decltype(value_rule)::value_type v)
    {
        if(v.index() == 0)
            return unquote_string(get<0>(v));

        return std::string{ get<1>(v) };
    };

    form_option_result rs;

    rs.name   = name;
    rs.value  = extract(value);
    rs.prefix = prefix.value_or("\0").at(0);

    for(auto [name, value] : range)
    {
        if(name == "filename")
            rs.filename = extract(value);
        else if(name == "type")
            rs.type = extract(value);
        else if(name == "headers")
            rs.headers.push_back(extract(value));
        else
            throw std::runtime_error{ "Illegally formatted input field" };
    }

    return rs;
}
