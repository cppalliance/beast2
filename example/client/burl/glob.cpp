//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include "glob.hpp"

#include <boost/optional.hpp>
#include <boost/url.hpp>

#include <cstdint>
#include <variant>

namespace urls    = boost::urls;
namespace grammar = urls::grammar;

namespace
{
using return_t = boost::optional<std::string>;

auto
make_range_gen(
    std::uint8_t width,
    std::uint64_t low,
    std::uint64_t high,
    std::uint64_t step)
{
    return [width, low, high, step]() mutable -> return_t
    {
        if(low > high)
            return boost::none;

        auto rs = std::to_string(std::exchange(low, low + step));
        if(width > rs.size())
            rs.insert(0, width - rs.size(), '0');
        return rs;
    };
};

auto
make_char_gen(char low, char high, std::uint8_t step)
{
    return [low, high, step]() mutable -> return_t
    {
        if(low > high)
            return boost::none;

        return std::string{ std::exchange(low, low + step) };
    };
};

auto
make_set_gen(std::vector<std::string> items)
{
    return [items = std::move(items), i = std::size_t{}]() mutable -> return_t
    {
        if(i == items.size())
            return boost::none;

        return items[i++];
    };
};

auto
make_static_gen(std::string s)
{
    return [s = return_t{ std::move(s) }]() mutable
    {
        return std::exchange(s, boost::none);
    };
};

using variant_t = std::variant<
    decltype(make_range_gen(0, 0, 0, 0)),
    decltype(make_char_gen(0, 0, 0)),
    decltype(make_set_gen({})),
    decltype(make_static_gen({}))>;

constexpr bool
holds_static_gen(const variant_t& v) noexcept
{
    return std::holds_alternative<decltype(make_static_gen({}))>(v);
}
} // namespace

std::function<boost::optional<glob_result>()>
make_glob_generator(core::string_view pattern)
{
    static constexpr auto static_rule =
        grammar::token_rule(grammar::all_chars - grammar::lut_chars("{["));

    static constexpr auto num_range_rule = grammar::tuple_rule(
        grammar::squelch(grammar::delim_rule('[')),
        grammar::token_rule(grammar::digit_chars),
        grammar::squelch(grammar::delim_rule('-')),
        grammar::unsigned_rule<std::uint64_t>(),
        grammar::optional_rule(
            grammar::tuple_rule(
                grammar::squelch(grammar::delim_rule(':')),
                grammar::unsigned_rule<std::uint64_t>())),
        grammar::squelch(grammar::delim_rule(']')));

    static constexpr auto char_range_rule = grammar::tuple_rule(
        grammar::squelch(grammar::delim_rule('[')),
        grammar::delim_rule(grammar::alpha_chars),
        grammar::squelch(grammar::delim_rule('-')),
        grammar::delim_rule(grammar::alpha_chars),
        grammar::optional_rule(
            grammar::tuple_rule(
                grammar::squelch(grammar::delim_rule(':')),
                grammar::unsigned_rule<std::uint8_t>())),
        grammar::squelch(grammar::delim_rule(']')));

    static constexpr auto set_rule = grammar::tuple_rule(
        grammar::squelch(grammar::delim_rule('{')),
        grammar::range_rule(
            grammar::tuple_rule(
                grammar::token_rule(
                    grammar::all_chars - grammar::lut_chars(",}")),
                grammar::squelch(
                    grammar::optional_rule(grammar::delim_rule(','))))),
        grammar::squelch(grammar::delim_rule('}')));

    static constexpr auto glob_rule = grammar::range_rule(
        grammar::variant_rule(
            static_rule, num_range_rule, char_range_rule, set_rule));

    const auto parse_rs = parse(pattern, glob_rule);

    if(parse_rs.has_error())
        throw std::runtime_error{ "Bad URL pattern" };

    std::vector<variant_t> gs;

    for(auto v : parse_rs.value())
    {
        switch(v.index())
        {
        case 0:
        {
            gs.push_back(make_static_gen(get<0>(v)));
            break;
        }
        case 1:
        {
            auto [low, high, step] = get<1>(v);
            gs.push_back(make_range_gen(
                static_cast<std::uint8_t>(low.size()),
                std::stoull(low),
                high,
                step.value_or(std::uint64_t{ 1 })));
            break;
        }
        case 2:
        {
            auto [low, high, step] = get<2>(v);
            gs.push_back(make_char_gen(
                low.at(0), high.at(0), step.value_or(std::uint8_t{ 1 })));
            break;
        }
        case 3:
        {
            auto items = std::vector<std::string>{};
            for(auto s : get<3>(v))
                items.push_back(s);
            gs.push_back(make_set_gen(std::move(items)));
            break;
        }
        }
    }

    using stack_t  = std::vector<std::pair<std::string, decltype(gs)>>;
    using tokens_t = std::vector<std::string>;
    return [stack  = stack_t{ { {}, std::move(gs) } },
            tokens = tokens_t{}]() mutable
    {
        while(!stack.empty())
        {
            auto& [prefix, gs] = stack.back();

            if(auto o = std::visit([](auto& g) { return g(); }, gs.front()))
            {
                if(gs.size() == 1)
                {
                    auto tmp = tokens;
                    if(!holds_static_gen(gs.front()))
                        tmp.push_back(o.value());
                    return boost::optional<glob_result>(
                        { prefix + o.value(), std::move(tmp) });
                }

                if(!holds_static_gen(gs.front()))
                    tokens.push_back(o.value());

                stack.emplace_back(
                    prefix + o.value(),
                    decltype(gs){ std::next(gs.begin()), gs.end() });
            }
            else
            {
                stack.pop_back();
                if(!stack.empty() &&
                   !holds_static_gen(stack.back().second.front()))
                    tokens.pop_back();
            }
        }
        return boost::optional<glob_result>{};
    };
}

std::string
glob_result::interpolate(core::string_view format)
{
    static constexpr auto rule = grammar::range_rule(
        grammar::variant_rule(
            grammar::token_rule(grammar::all_chars - grammar::lut_chars("#")),
            grammar::tuple_rule(
                grammar::squelch(grammar::delim_rule('#')),
                grammar::token_rule(grammar::digit_chars)),
            grammar::tuple_rule(grammar::squelch(grammar::delim_rule('#')))));

    const auto parse_rs = parse(format, rule);

    std::string rs;
    for(auto v : parse_rs.value())
    {
        switch(v.index())
        {
        case 0:
        {
            rs.append(get<0>(v));
            break;
        }
        case 1:
        {
            auto index = std::stoul(get<1>(v)) - 1;
            if(index < tokens.size())
            {
                rs.append(tokens[index]);
            }
            else
            {
                rs.push_back('#');
                rs.append(get<1>(v));
            }
            break;
        }
        case 2:
        {
            rs.push_back('#');
            break;
        }
        }
    }
    return rs;
}
