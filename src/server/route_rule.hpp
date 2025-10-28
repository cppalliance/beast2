//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_ROUTE_RULE_HPP
#define BOOST_BEAST2_SERVER_ROUTE_RULE_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/url/grammar/alpha_chars.hpp>
#include <boost/url/grammar/charset.hpp>
#include <boost/url/grammar/parse.hpp>
#include <vector>

namespace boost {
namespace beast2 {

namespace grammar = urls::grammar;

//------------------------------------------------

/** Rule for parsing a non-empty token of chars

    @par Requires
    @code
    std::is_empty<CharSet>::value == true
    @endcode
*/
template<class CharSet>
struct token_rule
{
    using value_type = core::string_view;

    auto
    parse(
        char const*& it,
        char const* end) const noexcept ->
            system::result<value_type>
    {
        static_assert(std::is_empty<CharSet>::value, "");
        if(it == end)
            return grammar::error::syntax;
        auto it1 = grammar::find_if_not(it, end, CharSet{});
        if(it1 == it)
            return grammar::error::mismatch;
        auto s = core::string_view(it, it1);
        it = it1;
        return s;
    }
};

//------------------------------------------------

/*
route-pattern     =  *( "/" segment ) [ "/" ]
segment           = literal-segment / param-segment
literal-segment   = 1*( unreserved-char )
param-segment     = param-prefix param-name [ constraint ] [ modifier ]
param-prefix      = ":" / "*"     ; either named param ":" or named wildcard "*"
param-name        = ident
constraint        = "(" 1*( constraint-char ) ")"
modifier          = "?" / "*" / "+"
ident             = ALPHA *( ALPHA / DIGIT / "_" )
constraint-char   = %x20-7E except ( ")" )
unreserved-char   = %x21-2F / %x30-3B / %x3D-5A / %x5C-7E  ; all printable except slash
*/

//------------------------------------------------

struct unreserved_char
{
    constexpr
    bool
    operator()(char ch) const noexcept
    {
        return ch != '/' && (
            (ch >= 0x21 && ch <= 0x2F) ||
            (ch >= 0x30 && ch <= 0x3B) ||
            (ch >= 0x3D && ch <= 0x5A) ||
            (ch >= 0x5C && ch <= 0x7E));
    }
};

struct constraint_char
{
    constexpr
    bool
    operator()(char ch) const noexcept
    {
        return ch >= 0x20 && ch <= 0x7E && ch != ')';
    }
};

struct ident_char
{
    constexpr
    bool
    operator()(char ch) const noexcept
    {
        return
            (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') ||
            (ch >= 'A' && ch <= 'Z') ||
            (ch == '_');
    }
};

constexpr struct
{
    // empty for no constraint
    using value_type = core::string_view;

    auto
    parse(
        char const*& it,
        char const* end) const noexcept ->
            system::result<value_type>
    {
        if(it == end || *it != '(')
            return "";
        if(it == end)
            BOOST_BEAST2_RETURN_EC(
                grammar::error::syntax);
        auto it0 = it;
        it = grammar::find_if_not(
            it, end, constraint_char{});
        if(it - it0 <= 1)
        {
            // too small
            it = it0;
            BOOST_BEAST2_RETURN_EC(
                grammar::error::syntax);
        }
        if(it == end)
        {
            it = it0;
            BOOST_BEAST2_RETURN_EC(
                grammar::error::syntax);
        }
        if(*it != ')')
        {
            it0 = it;
            BOOST_BEAST2_RETURN_EC(
                grammar::error::syntax);
        }
        return core::string_view(++it0, it++);
    }
} constraint_rule{};

constexpr struct
{
    using value_type = core::string_view;

    auto
    parse(
        char const*& it,
        char const* end) const noexcept ->
            system::result<value_type>
    {
        if(it == end)
            BOOST_BEAST2_RETURN_EC(
                grammar::error::syntax);
        if(! grammar::alpha_chars(*it))
            BOOST_BEAST2_RETURN_EC(
                grammar::error::syntax);
        auto it0 = it++;
        it = grammar::find_if_not(
            it, end, ident_char{});
        return core::string_view(it0, it);
    }
} param_name_rule{};

//------------------------------------------------

struct param_segment_rule_t
{
    struct value_type
    {
        core::string_view s;
        core::string_view name;
        core::string_view constraint;
        char prefix = 0; // param-prefix or 0
        char modifier = 0;
    };

    auto
    parse(
        char const*& it,
        char const* end) const noexcept ->
            system::result<value_type>
    {
        if(it == end)
            BOOST_BEAST2_RETURN_EC(
                grammar::error::syntax);
        if(*it != ':' && *it != '*')
            BOOST_BEAST2_RETURN_EC(
                grammar::error::mismatch);
        value_type v;
        v.prefix = *it++;
        {
            // param-name
            auto rv = grammar::parse(
                it, end, param_name_rule);
            if(rv.has_error())
                return rv.error();
            v.name = rv.value();
        }
        {
            // constraint
            auto rv = grammar::parse(
                it, end, constraint_rule);
            if( rv.has_error())
                return rv.error();
            v.constraint = rv.value();
        }
        // modifier
        if( it != end && (
            *it == '?' || *it == '*' || *it == '+'))
            v.modifier = *it++;
        return v;
    }
};

constexpr param_segment_rule_t param_segment_rule{};

//------------------------------------------------

constexpr token_rule<unreserved_char> literal_segment_rule{};

struct segment_rule_t
{
    using value_type = param_segment_rule_t::value_type;

    auto
    parse(
        char const*& it,
        char const* end) const noexcept ->
            system::result<value_type>
    {
        if(it == end)
            return grammar::error::syntax;
        if(*it == ':' || *it == '*')
            return grammar::parse(
                it, end, param_segment_rule);
        auto rv = grammar::parse(
            it, end, literal_segment_rule);
        if(rv.has_error())
            return rv.error();
        value_type v;
        v.s = rv.value();
        return v;
    }
};

constexpr segment_rule_t segment_rule{};

//------------------------------------------------

// avoids SBO
class stable_chars
{
    char const* p_ = 0;
    std::size_t n_ = 0;

public:
    ~stable_chars()
    {
        if(p_)
            delete p_;
    }

    stable_chars() = default;

    stable_chars(
        stable_chars&& other) noexcept
        : p_(other.p_)
        , n_(other.n_)
    {
        other.p_ = nullptr;
        other.n_ = 0;
    }

    stable_chars& operator=(
        stable_chars&& other) noexcept
    {
        auto p = p_;
        auto n = n_;
        p_ = other.p_;
        n_ = other.n_;
        other.p_ = p;
        other.n_ = n;
        return *this;
    }

    explicit
    stable_chars(
        core::string_view s)
        : p_(
            [&]
            {
                auto p =new char[s.size()];
                std::memcpy(p, s.data(), s.size());
                return p;
            }())
        , n_(s.size())
    {
    }

    stable_chars(
        char const* it, char const* end)
        : stable_chars(core::string_view(it, end))
    {
    }

    char const* data() const noexcept
    {
        return p_;
    }

    std::size_t size() const noexcept
    {
        return n_;
    }
};

struct path_rule_t
{
    struct value_type
    {
        std::vector<segment_rule_t::value_type> v;
        stable_chars s;
    };

    auto
    parse(
        char const*& it0,
        char const* end0) const ->
            system::result<value_type>
    {
        value_type v;
        v.s = stable_chars(it0, end0);
        auto it = v.s.data();
        auto const end = it + v.s.size();
        do
        {
            auto rv = grammar::parse(
                it, end, next_rule{});
            if(rv.has_error())
                return rv.error();
            v.v.emplace_back(*rv);
        }
        while(it != end);
        it0 = end0;
        // gcc 7 bug workaround
        return system::result<value_type>(std::move(v));
    }

private:
    struct next_rule
    {
        using value_type = segment_rule_t::value_type;

        auto
        parse(
            char const*& it,
            char const* end) const ->
                system::result<value_type>
        {
            if(it == end)
                return grammar::error::end_of_range;
            if (*it != '/')
                return grammar::error::mismatch;
            ++it;
            if(it == end)
            {
                // trailing "/"
                return value_type{};
            }
            return grammar::parse(it, end, segment_rule);
        }
    };
};

constexpr path_rule_t path_rule{};

} // beast2
} // boost

#endif
