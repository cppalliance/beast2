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
#include <boost/beast2/server/detail/any_router.hpp>
#include <boost/beast2/error.hpp>
#include <boost/beast2/detail/except.hpp>
#include <boost/url/grammar/hexdig_chars.hpp>
#include <boost/assert.hpp>
#include <atomic>
#include <string>
#include <vector>

namespace boost {
namespace beast2 {
namespace detail {

any_router::any_handler::~any_handler() = default;

//------------------------------------------------

namespace {

// decode all percent escapes
std::string
pct_decode(
    urls::pct_string_view s)
{
    return core::string_view(s); // for now
}

// decode all percent escapes except slashes '/' and '\'
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
            return result;
        if(*it != '%')
        {
            result.push_back(*it++);
            continue;
        }
        if(++it == end)
            break;
        auto d0 = urls::grammar::hexdig_value(*it++);
        if( d0 < 0 ||
            it == end)
            break;
        auto d1 = urls::grammar::hexdig_value(*it++);
        if(d1 < 0)
            break;
        char c = d0 * 4 + d1;
        if( c != '/' &&
            c != '\\')
        {
            result.push_back(c);
            continue;
        }
        result.append(it - 3, 3);
    }
    detail::throw_invalid_argument(
        "bad percent encoding");
}

struct match_result
{
    std::size_t n = 0; // chars moved from path to base_path
};

} // (anon)

//------------------------------------------------

struct any_router::layer
{
    std::string pat_;            // prefix to match
    handler_ptr handler_;
    bool end_;                   // if an exact match is required
    //bool ignore_case = false;
    //bool strict = false;        // trailing slashes are significant
    // route r;
    path_rule_t::value_type pv_;

    layer(
        core::string_view pat,
        handler_ptr handler,
        bool end)
        : pat_(pct_decode(pat))
        , handler_(std::move(handler))
        , end_(end)
        , pv_(grammar::parse(
            pat, path_rule).value())
    {
    }

    /** Return true if path matches this layer

        Caller is responsible for selectively percent
        decoding the path. Slashes should not be decoded.
    */
    bool
    match(
        core::string_view path,
        match_result& mr,
        req_info const& ri) const
    {
        BOOST_ASSERT(! path.empty());
        auto it = path.data();
        auto const end = it + path.size();
        auto pit = pv_.segs.begin();
        auto const pend = pv_.segs.end();
        while(it != end && pit != pend)
        {
            if(! core::string_view(
                    it, end).starts_with(pit->prefix))
                return false;
            it += pit->prefix.size();
            ++pit;
        }
        if( end_ && (
            it != end ||
            pit != pend))
            return false;
        if( ! end_ &&
            pit != pend)
            return false;
        mr.n = it - path.data();
        ri.base_path = {
            ri.base_path.data(),
            ri.base_path.size() + mr.n };
        ri.path.remove_prefix(mr.n);
        return true;
    }
};

struct any_router::impl
{
    std::atomic<std::size_t> refs{1};
    std::vector<layer> layers;
    req_info(*get_req_info)(void*);
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

any_router::
any_router(
    req_info(*get_req_info)(void*))
    : impl_(new impl)
{
    impl_->get_req_info = get_req_info;
}

std::size_t
any_router::
count() const noexcept
{
    std::size_t n = 1;
    for(auto const& e : impl_->layers)
        n += e.handler_->count();
    return n;
}

// top-level dispatch gets called first
route_result
any_router::
dispatch_impl(
    http_proto::method verb,
    urls::url_view const& url,
    void* req, void*res, route_state& st) const
{
    st = {};
    st.verb = verb;
    st.verb_str = {};
    // VFALCO use reusing-StringToken
    st.decoded_path =
        pct_decode_path(url.encoded_path());

    auto ri = impl_->get_req_info(req);
    ri.base_path = { st.decoded_path.data(), 0 };
    ri.path = st.decoded_path;

    return dispatch_impl(req, res, st);
}

// recursive dispatch
route_result
any_router::
dispatch_impl(
    void* req, void* res, route_state& st) const
{
    auto ri = impl_->get_req_info(req);
    //system::error_code ec;

    for(auto const& e : impl_->layers)
    {
        match_result mr;
        if(e.match(ri.path, mr, ri))
        {
            auto rv = e.handler_->invoke(req, res, st, nullptr);
            if( rv.failed() ||
                rv == route::send ||
                rv == route::done ||
                rv == route::close
                // || rv == route::detach // VFALCO this would be best
                )
                return rv;
            if(rv == route::detach)
            {
                // do detach
                return rv;
            }
            // must be a non-success error code
            if(&rv.category() != &detail::route_cat)
                detail::throw_invalid_argument();

        }
    }

    return route::next;

#if 0
    route_result rv;
    system::error_code ec;
    auto ri = impl_->get_req_info(req);
    std::string path_str = std::string(ri.path->buffer());

    auto it = impl_->layers.begin();
    auto const end = impl_->layers.end();

    if(st.resume != 0)
    {
        // st.resume is how many we have to skip
        while(it != end)
        {
            auto const n = it->handler->count();
            if(st.pos + n <= st.resume)
            {
                // skip route and children
                st.pos += n;
                ++it;
                continue;
            }

            ++st.pos;

            if(st.pos == st.resume)
            {
                st.resume = 0;
                return st.ec;
            }
            break;
        }
    }

    for(;it != end;++it)
    {
        // 1-based index of last route we checked
        ++st.pos;

        if( it->method != http_proto::method::unknown &&
            ri.method != it->method)
            continue;
        if(! it->exact_prefix.empty())
        {
            if(it->prefix)
            {
                // only the prefix has to match
                if(! core::string_view(*ri.suffix_path).starts_with(
                    core::string_view(it->exact_prefix)))
                    continue;
            }
            else
            {
                // full match
                if( core::string_view(*ri.suffix_path) !=
                    core::string_view(it->exact_prefix))
                    continue;
            }
        }

        auto const base_path0 = *ri.base_path;
        auto const suffix_path0 = *ri.suffix_path;
        ri.base_path->append(
            ri.suffix_path->data(), it->exact_prefix.size());
        ri.suffix_path->erase(0, it->exact_prefix.size());
        // invoke the handler
        rv = it->handler->invoke(req, res, st, nullptr);
        *ri.base_path = base_path0;
        *ri.suffix_path = suffix_path0;

        if(rv == route::next)
            continue;
        if(&rv.category() != &detail::route_cat)
        {
            // must indicate failure
            if(! ec.failed())
                detail::throw_invalid_argument();
            ec = rv;
            goto do_error;
        }
        if(rv == route::detach)
        {
            // VFALCO this statement is broken, because a foreign
            // thread could be resuming and race with st.resume
            if( st.resume == 0)
                st.resume = st.pos;
            return rv;
        }
        BOOST_ASSERT(
            rv == route::send ||
            rv == route::close |
            rv == route::done);
        return rv;
    }
    return route::next;

do_error:
    while(++it != end)
    {
        // invoke error handlers
        rv = it->handler->invoke(req, res, st, &ec);
        if(&ec.category() == &detail::route_cat)
        {
            // can't change ec to a route result
            detail::throw_invalid_argument();
        }
        if(! ec.failed())
        {
            // can't change ec to success
            detail::throw_invalid_argument();
        }
        if(rv == route::next)
            continue;
        if( rv == route::send ||
            rv == route::close)
            return rv;
        if(rv == route::done)
        {
            // can't complete the response in handler
            detail::throw_invalid_argument();
        }
        if(rv == route::detach)
        {
            // can't detach in error handler
            detail::throw_invalid_argument();
        }
        // can't get here
        detail::throw_logic_error();
    }
    return ec;
#endif
}

auto
any_router::
resume(
    void* req, void* res, route_state& st,
    route_result const& ec) const ->
        route_result
{
    BOOST_ASSERT(st.resume > 0);
    if( ec == route::send ||
        ec == route::close ||
        ec == route::done)
        return ec;
    if(ec == route::detach)
    {
        // can't detach on resume
        detail::throw_invalid_argument();
    }
    if(&ec.category() != &detail::route_cat)
    {
        // must indicate failure
        if(! ec.failed())
            detail::throw_invalid_argument();
    }
    st.ec = ec;
    st.pos = 0;
    return dispatch_impl(req, res, st);
}

void
any_router::
append(
    bool end,
    http_proto::method method,
    core::string_view pat,
    handler_ptr h)
{
    // delete the last layer if it is empty, to handle the case where
    // the user calls route() without actually adding anything after.
    /*
    if( ! impl_->layers.empty() &&
        ! impl_->layers.back().handler)
    {
        --impl_->size;
        impl_->layers.pop_back();
    }*/
    (void)method;
    impl_->layers.emplace_back(
        pat, std::move(h), end);
}

void
any_router::
append(
    core::string_view,
    any_router&)
{
}

} // detail
} // beast2
} // boost
