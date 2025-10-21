//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include <boost/http_io/server/router.hpp>
#include <boost/url/grammar/alpha_chars.hpp>
#include <boost/url/grammar/charset.hpp>
#include <boost/url/grammar/lut_chars.hpp>
#include <boost/url/grammar/parse.hpp>
#include <boost/url/grammar/range_rule.hpp>
#include <map>
#include <string>
#include <vector>

namespace boost {
namespace http_io {

namespace grammar = urls::grammar;

//------------------------------------------------

namespace {

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

struct constraint_char
{
    constexpr
    bool
    operator()(char ch) const noexcept
    {
        return ch >= 0x20 && ch <= 0x7E && ch != ')';
    }
};

constexpr grammar::lut_chars unreserved_chars(unreserved_char{});

struct ident_rule
{
    using value_type = core::string_view;

    auto
    parse(
        char const*& it,
        char const* end) const noexcept ->
            system::result<value_type>
    {
        if(it == end)
        {
            BOOST_HTTP_PROTO_RETURN_EC(
                grammar::error::syntax);
        }
        if(! grammar::alpha_chars(*it))
        {
            BOOST_HTTP_PROTO_RETURN_EC(
                grammar::error::syntax);
        }
        auto it0 = it++;
        it = grammar::find_if_not(
            it, end, ident_char{});
        return core::string_view(it0, it);
    }
};

struct segment_rule
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
        {
            BOOST_HTTP_PROTO_RETURN_EC(
                grammar::error::need_more);
        }
        value_type v;
        auto it0 = it;
        if(*it != ':' && *it != '*')
        {
            // literal_segment
            it = grammar::find_if_not(
                it, end, unreserved_chars);
            if(it == it0)
            {
                BOOST_HTTP_PROTO_RETURN_EC(
                    grammar::error::mismatch);
            }
            v.s = { it0, it };
            return v;
        }
        // param-segment
        v.prefix = *it++;
        auto rv = grammar::parse(it, end, ident_rule{});
        if(rv.has_error())
            return rv.error();
        v.name = rv.value();
        // constraint
        if(it != end && *it == '(')
        {
            ++it;
            auto it1 = it;
            it = grammar::find_if_not(
                it, end, constraint_char{});
            if(it == it1)
            {
                it = it0;
                BOOST_HTTP_PROTO_RETURN_EC(
                    grammar::error::syntax);
            }
            if(it == end || *it != ')')
            {
                it = it0;
                BOOST_HTTP_PROTO_RETURN_EC(
                    grammar::error::syntax);
            }
            v.constraint = { it1, it };
            ++it;
        }
        // modifier
        if( it != end &&
            *it == '?' || *it == '*' || *it == '+')
            v.modifier = *it++;
        return v;
    }
};

struct path_rule
{
    using value_type =
        grammar::range<segment_rule::value_type>;

    auto
    parse(
        char const*& it,
        char const* end) const ->
            system::result<value_type>
    {
        return grammar::parse(it, end,
            grammar::range_rule(next_rule{}));
    }

private:
    struct next_rule
    {
        using value_type = segment_rule::value_type;

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
            auto it0 = it;
            ++it;
            if(it == end)
            {
                // trailing "/"
                return value_type{};
            }
            return grammar::parse(it, end, segment_rule{});
        }
    };
};

} // (anon)

//------------------------------------------------

router_base::any_handler::~any_handler() = default;

//------------------------------------------------

struct router_base::entry
{
    http_proto::method method;
    std::vector<handler_ptr> v;
};

struct router_base::impl
{
    http_proto::method(*get_method)(void*);
    urls::segments_encoded_view&(*get_path)(void*);
    std::vector<handler_ptr> v0;
    std::vector<entry> v;
};

//------------------------------------------------

router_base::
router_base(
    http_proto::method(*get_method)(void*),
    urls::segments_encoded_view&(*get_path)(void*))
    : impl_(std::make_shared<impl>())
{
    impl_->get_method = get_method;
    impl_->get_path = get_path;
}

void
router_base::
use(
    handler_ptr h)
{
    impl_->v0.emplace_back(std::move(h));
}

void
router_base::
insert(
    http_proto::method method,
    core::string_view path,
    handler_ptr h)
{
    char const* it = path.data();
    char const* const end = it + path.size();
    auto rv = grammar::parse(it, end, path_rule{});
    path_rule::value_type v;
    if(rv.has_value())
        v = rv.value();

    std::vector<handler_ptr> vh;
    vh.emplace_back(std::move(h));

    impl_->v.emplace_back(entry{
        method,
        std::move(vh)});
}

//------------------------------------------------

bool
router_base::
invoke(
    void* req, void* res) const
{
    // global handlers
    for(auto const& r : impl_->v0)
    {
        if(! r->operator()(req, res))
            return false;
    }

    auto method = impl_->get_method(req);
    auto& path = impl_->get_path(req);
    for(auto const& r : impl_->v)
    {
        if(r.method != method &&
            method != http_proto::method::unknown)
            continue;
        for(auto& e : r.v)
            if(! e->operator()(req, res))
                return false;
    }

    return true;
}

} // http_io
} // boost
