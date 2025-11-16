//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include "src/server/route_rule.hpp"
#include <boost/beast2/server/basic_router.hpp>
#include <boost/beast2/server/route_handler.hpp>
#include <boost/beast2/error.hpp>
#include <boost/beast2/detail/except.hpp>
#include <boost/url/grammar/ci_string.hpp>
#include <boost/url/grammar/hexdig_chars.hpp>
#include <boost/assert.hpp>
#include <atomic>
#include <string>
#include <vector>

namespace boost {
namespace beast2 {

namespace detail {

// VFALCO Temporarily here until Boost.URL merges the fix
static
bool
ci_is_equal(
    core::string_view s0,
    core::string_view s1) noexcept
{
    auto n = s0.size();
    if(s1.size() != n)
        return false;
    auto p1 = s0.data();
    auto p2 = s1.data();
    char a, b;
    // fast loop
    while(n--)
    {
        a = *p1++;
        b = *p2++;
        if(a != b)
            goto slow;
    }
    return true;
    do
    {
        a = *p1++;
        b = *p2++;
    slow:
        if( grammar::to_lower(a) !=
            grammar::to_lower(b))
            return false;
    }
    while(n--);
    return true;
}


//------------------------------------------------
/*

pattern     target      path(use)    path(get) 
-------------------------------------------------
/           /           /
/           /api        /api
/api        /api        /            /api
/api        /api/       /            /api/
/api        /api/       /            no-match       strict
/api        /api/v0     /v0          no-match
/api/       /api        /            /api
/api/       /api        /            no-match       strict
/api/       /api/       /            /api/
/api/       /api/v0     /v0          no-match

*/

//------------------------------------------------

any_router::any_handler::~any_handler() = default;

//------------------------------------------------

/*
static
void
make_lower(std::string& s)
{
    for(auto& c : s)
        c = grammar::to_lower(c);
}
*/

// decode all percent escapes
static
std::string
pct_decode(
    urls::pct_string_view s)
{
    std::string result;
    core::string_view sv(s);
    result.reserve(s.size());
    auto it = sv.data();
    auto const end = it + sv.size();
    for(;;)
    {
        if(it == end)
            break;
        if(*it != '%')
        {
            result.push_back(*it++);
            continue;
        }
        ++it;
#if 0
        // pct_string_view can never have invalid pct-encodings
        if(it == end)
            goto invalid;
#endif
        auto d0 = urls::grammar::hexdig_value(*it++);
#if 0
        // pct_string_view can never have invalid pct-encodings
        if( d0 < 0 ||
            it == end)
            goto invalid;
#endif
        auto d1 = urls::grammar::hexdig_value(*it++);
#if 0
        // pct_string_view can never have invalid pct-encodings
        if(d1 < 0)
            goto invalid;
#endif
        result.push_back(d0 * 16 + d1);
    }
    return result;
#if 0
invalid:
    // can't get here, as received a pct_string_view
    detail::throw_invalid_argument();
#endif
}

// decode all percent escapes except slashes '/' and '\'
static
std::string
pct_decode_path(
    urls::pct_string_view s)
{
    std::string result;
    core::string_view sv(s);
    result.reserve(s.size());
    auto it = sv.data();
    auto const end = it + sv.size();
    for(;;)
    {
        if(it == end)
            break;
        if(*it != '%')
        {
            result.push_back(*it++);
            continue;
        }
        ++it;
#if 0
        // pct_string_view can never have invalid pct-encodings
        if(it == end)
            goto invalid;
#endif
        auto d0 = urls::grammar::hexdig_value(*it++);
#if 0
        // pct_string_view can never have invalid pct-encodings
        if( d0 < 0 ||
            it == end)
            goto invalid;
#endif
        auto d1 = urls::grammar::hexdig_value(*it++);
#if 0
        // pct_string_view can never have invalid pct-encodings
        if(d1 < 0)
            goto invalid;
#endif
        char c = d0 * 16 + d1;
        if( c != '/' &&
            c != '\\')
        {
            result.push_back(c);
            continue;
        }
        result.append(it - 3, 3);
    }
    return result;
#if 0
invalid:
    // can't get here, as received a pct_string_view
    detail::throw_invalid_argument();
#endif
}

//------------------------------------------------

} // detail

struct basic_request::
    match_result
{
    void adjust_path(
        basic_request& req,
        std::size_t n)
    {
        n_ = n;
        if(n_ == 0)
            return;
        req.base_path = {
            req.base_path.data(),
            req.base_path.size() + n_ };
        if(n_ < req.path.size())
        {
            req.path.remove_prefix(n_);
        }
        else
        {
            // append a soft slash
            req.path = { req.decoded_path_.data() +
                req.decoded_path_.size() - 1, 1};
            BOOST_ASSERT(req.path == "/");
        }
    }

    void restore_path(
        basic_request& req)
    {
        if( n_ > 0 &&
            req.addedSlash_ &&
            req.path.data() ==
                req.decoded_path_.data() +
                req.decoded_path_.size() - 1)
        {
            // remove soft slash
            req.path = {
                req.base_path.data() +
                req.base_path.size(), 0 };
        }
        req.base_path.remove_suffix(n_);
        req.path = {
            req.path.data() - n_,
            req.path.size() + n_ };
    }

private:
    std::size_t n_ = 0; // chars moved from path to base_path
};

//------------------------------------------------

namespace detail {

// Matches a path against a pattern
struct any_router::matcher
{
    bool const end; // false for middleware

    matcher(
        core::string_view pat,
        bool end_)
        : end(end_)
        , decoded_pat_(
            [&pat]
            {
                auto s = pct_decode(pat);
                if( s.size() > 1
                    && s.back() == '/')
                    s.pop_back();
                return s;
            }())
        , slash_(pat == "/")
    {
        if(! slash_)
            pv_ = grammar::parse(
                decoded_pat_, path_rule).value();
    }

    /** Return true if req.path is a match
    */
    bool operator()(
        basic_request& req,
        match_result& mr) const
    {
        BOOST_ASSERT(! req.path.empty());
        if( slash_ && (
            ! end ||
            req.path == "/"))
        {
            // params = {};
            mr.adjust_path(req, 0);
            return true;
        }
        auto it = req.path.data();
        auto pit = pv_.segs.begin();
        auto const end_ = it + req.path.size();
        auto const pend = pv_.segs.end();
        while(it != end_ && pit != pend)
        {
            // prefix has to match
            auto s = core::string_view(it, end_);
            if(! req.case_sensitive)
            {
                if(pit->prefix.size() > s.size())
                    return false;
                s = s.substr(0, pit->prefix.size());
                //if(! grammar::ci_is_equal(s, pit->prefix))
                if(! ci_is_equal(s, pit->prefix))
                    return false;
            }
            else
            {
                if(! s.starts_with(pit->prefix))
                    return false;
            }
            it += pit->prefix.size();
            ++pit;
        }
        if(end)
        {
            // require full match
            if( it != end_ ||
                pit != pend)
                return false;
        }
        else if(pit != pend)
        {
            return false;
        }
        // number of matching characters
        auto const n = it - req.path.data();
        mr.adjust_path(req, n);
        return true;
    }

private:
    stable_string decoded_pat_;
    path_rule_t::value_type pv_;
    bool slash_;
};

//------------------------------------------------

struct any_router::layer
{
    struct entry
    {
        handler_ptr handler;

        // only for end routes
        http_proto::method verb =
            http_proto::method::unknown;
        std::string verb_str;
        bool all;

        explicit entry(
            handler_ptr h) noexcept
            : handler(std::move(h))
            , all(true)
        {
        }

        entry(
            http_proto::method verb_,
            handler_ptr h) noexcept
            : handler(std::move(h))
            , verb(verb_)
            , all(false)
        {
            BOOST_ASSERT(verb !=
                http_proto::method::unknown);
        }

        entry(
            core::string_view verb_str_,
            handler_ptr h) noexcept
            : handler(std::move(h))
            , verb(http_proto::string_to_method(verb_str_))
            , all(false)
        {
            if(verb != http_proto::method::unknown)
                return;
            verb_str = verb_str_;
        }

        bool match_method(
            basic_request const& req) const noexcept
        {
            if(all)
                return true;
            if(verb != http_proto::method::unknown)
                return req.verb_ == verb;
            if(req.verb_ != http_proto::method::unknown)
                return false;
            return req.verb_str_ == verb_str;
        }
    };

    matcher match;
    std::vector<entry> entries;

    // middleware layer
    layer(
        core::string_view pat,
        handler_list handlers)
        : match(pat, false)
    {
        entries.reserve(handlers.n);
        for(std::size_t i = 0; i < handlers.n; ++i)
            entries.emplace_back(std::move(handlers.p[i]));
    }

    // route layer
    explicit layer(
        core::string_view pat)
        : match(pat, true)
    {
    }

    std::size_t count() const noexcept
    {
        std::size_t n = 0;
        for(auto const& e : entries)
            n += e.handler->count();
        return n;
    }   
};

//------------------------------------------------

struct any_router::impl
{
    std::atomic<std::size_t> refs{1};
    std::vector<layer> layers;
    opt_flags opt;

    explicit impl(
        opt_flags opt_) noexcept
        : opt(opt_)
    {
    }
};

//------------------------------------------------

any_router::
~any_router()
{
    if(! impl_)
        return;
    if(--impl_->refs == 0)
        delete impl_;
}

any_router::
any_router(
    opt_flags opt)
    : impl_(new impl(opt))
{
}

any_router::
any_router(any_router&& other) noexcept
    :impl_(other.impl_)
{
    other.impl_ = nullptr;
}

any_router::
any_router(any_router const& other) noexcept
{
    impl_ = other.impl_;
    ++impl_->refs;
}

any_router&
any_router::
operator=(any_router&& other) noexcept
{
    auto p = impl_;
    impl_ = other.impl_;
    other.impl_ = nullptr;
    if(p && --p->refs == 0)
        delete p;
    return *this;
}

any_router&
any_router::
operator=(any_router const& other) noexcept
{
    auto p = impl_;
    impl_ = other.impl_;
    ++impl_->refs;
    if(p && --p->refs == 0)
        delete p;
    return *this;
}

//------------------------------------------------

std::size_t
any_router::
count() const noexcept
{
    std::size_t n = 0;
    for(auto const& i : impl_->layers)
        for(auto const& e : i.entries)
            n += e.handler->count();
    return n;
}

auto
any_router::
new_layer(
    core::string_view pattern) -> layer&
{
    // the pattern must not be empty
    if(pattern.empty())
        detail::throw_invalid_argument();
    // delete the last route if it is empty,
    // this happens if they call route() without
    // adding anything
    if(! impl_->layers.empty() &&
        impl_->layers.back().entries.empty())
        impl_->layers.pop_back();
    impl_->layers.emplace_back(pattern);
    return impl_->layers.back();
};

void
any_router::
add_impl(
    core::string_view pattern,
    handler_list const& handlers)
{
    if( pattern.empty())
        pattern = "/";
    impl_->layers.emplace_back(
        pattern, std::move(handlers));
}

void
any_router::
add_impl(
    layer& e,
    http_proto::method verb,
    handler_list const& handlers)
{
    // cannot be unknown
    if(verb == http_proto::method::unknown)
        detail::throw_invalid_argument();

    e.entries.reserve(e.entries.size() + handlers.n);
    for(std::size_t i = 0; i < handlers.n; ++i)
        e.entries.emplace_back(verb,
            std::move(handlers.p[i]));
}

void
any_router::
add_impl(
    layer& e,
    core::string_view verb_str,
    handler_list const& handlers)
{
    e.entries.reserve(e.entries.size() + handlers.n);

    if(verb_str.empty())
    {
        // all
        for(std::size_t i = 0; i < handlers.n; ++i)
            e.entries.emplace_back(
                std::move(handlers.p[i]));
        return;
    }

    // possibly custom string
    for(std::size_t i = 0; i < handlers.n; ++i)
        e.entries.emplace_back(verb_str,
            std::move(handlers.p[i]));
}

//------------------------------------------------

auto
any_router::
resume_impl(
    basic_request& req, basic_response& res,
    route_result const& ec) const ->
        route_result
{
    BOOST_ASSERT(res.resume_ > 0);
    if( ec == route::send ||
        ec == route::close ||
        ec == route::complete)
        return ec;
    if(&ec.category() != &detail::route_cat)
    {
        // must indicate failure
        if(! ec.failed())
            detail::throw_invalid_argument();
    }

    // restore base_path and path
    req.base_path = { req.decoded_path_.data(), 0 };
    req.path = req.decoded_path_;
    if(req.addedSlash_)
        req.path.remove_suffix(1);

    // resume_ was set in the handler's wrapper
    BOOST_ASSERT(res.resume_ == res.pos_);
    res.pos_ = 0;
    res.ec_ = ec;
    return do_dispatch(req, res);
}

// top-level dispatch that gets called first
route_result
any_router::
dispatch_impl(
    http_proto::method verb,
    core::string_view verb_str,
    urls::url_view const& url,
    basic_request& req,
    basic_response& res) const
{
    // VFALCO we could reuse the string storage by not clearing them
    // set req.case_sensitive, req.strict to default of false
    req = {};
    if(verb == http_proto::method::unknown)
    {
        BOOST_ASSERT(! verb_str.empty());
        verb = http_proto::string_to_method(verb_str);
        if(verb == http_proto::method::unknown)
            req.verb_str_ = verb_str;
    }
    else
    {
        BOOST_ASSERT(verb_str.empty());
    }
    req.verb_ = verb;
    // VFALCO use reusing-StringToken
    req.decoded_path_ =
        pct_decode_path(url.encoded_path());
    BOOST_ASSERT(! req.decoded_path_.empty());
    req.base_path = { req.decoded_path_.data(), 0 };
    req.path = req.decoded_path_;
    if(req.decoded_path_.back() != '/')
    {
        req.decoded_path_.push_back('/');
        req.addedSlash_ = true;
    }
    BOOST_ASSERT(req.case_sensitive == false);
    BOOST_ASSERT(req.strict == false);

    res = {};

    // we cannot do anything after do_dispatch returns,
    // other than return the route_result, or else we
    // could race with the detached operation trying to resume.
    return do_dispatch(req, res);
}

// recursive dispatch
route_result
any_router::
dispatch_impl(
    basic_request& req,
    basic_response& res) const
{
    // options are recursive and need to be restored on
    // exception or when returning to a calling router.
    struct option_saver
    {
        option_saver(
            basic_request& req) noexcept
            : req_(&req)
            , case_sensitive_(req.case_sensitive)
            , strict_(req.strict)
        {
        }

        ~option_saver()
        {
            if(! req_)
                return;
            req_->case_sensitive = case_sensitive_;
            req_->strict = strict_;
        };

        void cancel() noexcept
        {
            req_ = nullptr;
        }

    private:
        basic_request* req_;
        bool case_sensitive_;
        bool strict_;
    };

    option_saver restore_options(req);

    // inherit or apply options
    if((impl_->opt & 2) != 0)
        req.case_sensitive = true;
    else if((impl_->opt & 4) != 0)
        req.case_sensitive = false;

    if((impl_->opt & 8) != 0)
        req.strict = true;
    else if((impl_->opt & 16) != 0)
        req.strict = false;

    match_result mr;
    for(auto const& i : impl_->layers)
    {
        if(res.resume_ > 0)
        {
            auto const n = i.count(); // handlers in layer
            if(res.pos_ + n < res.resume_)
            {
                res.pos_ += n; // skip layer
                continue;
            }
            // repeat match to recreate the stack
            bool is_match = i.match(req, mr);
            BOOST_ASSERT(is_match);
            (void)is_match;
        }
        else
        {
            if(i.match.end && res.ec_.failed())
            {
                // routes can't have error handlers
                res.pos_ += i.count(); // skip layer
                continue;
            }
            if(! i.match(req, mr))
            {
                // not a match
                res.pos_ += i.count(); // skip layer
                continue;
            }
        }
        for(auto it = i.entries.begin();
            it != i.entries.end(); ++it)
        {
            auto const& e(*it);
            if(res.resume_)
            {
                auto const n = e.handler->count();
                if(res.pos_ + n < res.resume_)
                {
                    res.pos_ += n; // skip entry
                    continue;
                }
                BOOST_ASSERT(e.match_method(req));
            }
            else if(i.match.end)
            {
                // check verb for match 
                if(! e.match_method(req))
                {
                    res.pos_ += e.handler->count(); // skip entry
                    continue;
                }
            }

            route_result rv;
            // increment before invoke
            ++res.pos_;
            if(res.pos_ != res.resume_)
            {
                // call the handler
                rv = e.handler->invoke(req, res);
                // res.pos_ can be incremented further
                // inside the above call to invoke.
                if(rv == route::detach)
                {
                    // It is essential that we return immediately, without
                    // doing anything after route::detach is returned,
                    // otherwise we could race with the detached operation
                    // attempting to call resume().
                    restore_options.cancel();
                    return rv;
                }
            }
            else
            {
                // a subrouter never detaches on its own
                BOOST_ASSERT(e.handler->count() == 1);
                // can't detach on resume
                if(res.ec_ == route::detach)
                    detail::throw_invalid_argument();
                // do resume
                res.resume_ = 0;
                rv = res.ec_;
                res.ec_ = {};
            }
            if( rv == route::send ||
                rv == route::complete ||
                rv == route::close)
                return rv;
            if(rv.failed())
            {
                // error handling mode
                res.ec_ = rv;
                if(! i.match.end)
                    continue; // next entry
                // routes don't have error handlers
                while(++it != i.entries.end())
                    res.pos_ += it->handler->count();
                break; // skip remaining entries
            }
            if(rv == route::next)
                continue; // next entry
            if(rv == route::next_route)
            {
                // middleware can't return next_route
                if(! i.match.end)
                    detail::throw_invalid_argument();
                while(++it != i.entries.end())
                    res.pos_ += it->handler->count();
                break; // skip remaining entries
            }
            // we must handle all route enums
            BOOST_ASSERT(&rv.category() != &detail::route_cat);
            // handler must return non-successful error_code
            detail::throw_invalid_argument();
        }

        mr.restore_path(req);
    }

    return route::next;
}

route_result
any_router::
do_dispatch(
    basic_request& req,
    basic_response& res) const
{
    auto rv = dispatch_impl(req, res);
    BOOST_ASSERT(&rv.category() == &detail::route_cat);
    BOOST_ASSERT(rv != route::next_route);
    if(rv != route::next)
    {
        // when rv == route::detach we must return immediately,
        // without attempting to perform any additional operations.
        return rv;
    }
    if(! res.ec_.failed())
    {
        // unhandled route
        return route::next;
    }
    // error condition
    return res.ec_;
}

} // detail
} // beast2
} // boost
