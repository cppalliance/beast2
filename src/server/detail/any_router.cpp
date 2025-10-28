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
#include <boost/beast2/server/detail/any_router.hpp>
#include <boost/beast2/error.hpp>
#include <boost/beast2/detail/except.hpp>
#include <map>
#include <string>
#include <vector>

namespace boost {
namespace beast2 {
namespace detail {

any_router::any_handler::~any_handler() = default;

any_router::any_errfn::~any_errfn() = default;

//------------------------------------------------

struct any_router::entry
{
    bool prefix = true;             // prefix match, for pathless use()
    http_proto::method method;      // method::unknown for all, ignored for use()
    std::string exact_prefix;       // VFALCO hack because true matching doesn't work
    path_rule_t::value_type pat;    // VFALCO not used yet
    handler_ptr handler;

    entry(
        bool prefix_,
        http_proto::method method_,
        core::string_view pat_,
        handler_ptr h = nullptr)
        : prefix(prefix_)
        , method(method_)
        , exact_prefix(pat_)
        , handler(std::move(h))
    {
        // pat = grammar::parse(it, end, path_rule).value(),
    }

    void append(handler_ptr h)
    {
        BOOST_ASSERT(! handler);
        handler = std::move(h);
    }
};

struct any_router::impl
{
    std::size_t size = 0;
    std::vector<entry> list;
    std::vector<errfn_ptr> errfns;
    http_proto::method(*get_method)(void*);
    urls::segments_encoded_view&(*get_path)(void*);
};

//------------------------------------------------

any_router::
any_router(
    http_proto::method(*get_method)(void*),
    urls::segments_encoded_view&(*get_path)(void*))
    : impl_(std::make_shared<impl>())
{
    impl_->get_path = get_path;
    impl_->get_method = get_method;
}

std::size_t
any_router::
size() const noexcept
{
    return impl_->size;
}

auto
any_router::
invoke(
    void* req, void* res, route_state& st) const ->
        system::error_code
{
    system::error_code ec;
    auto method = impl_->get_method(req);
    auto& path = impl_->get_path(req);
    std::string path_str = std::string(path.buffer());

    // routers don't detach
    //BOOST_ASSERT(st.pos != st.resume);

    // loop until the request is handled
    for(auto const& e : impl_->list)
    {
        if(st.resume == 0)
        {
            ++st.pos;

            if( e.method != http_proto::method::unknown &&
                method != e.method)
                continue;

            if(e.prefix)
            {
                // only the prefix has to match
                if(! core::string_view(path_str).starts_with(
                        core::string_view(e.exact_prefix)))
                    continue;
            }
            else
            {
                // check for full match
                //if(! match(path, e.pat))
                //  continue;
            }
        }
        else
        {
            // resuming

            // VFALCO this is broken
            if(st.pos + e.handler->n_routes < st.resume)
            {
                // skip on resume
                st.pos += e.handler->n_routes;
                continue;
            }

            ++st.pos;

            if(st.pos == st.resume)
            {
                st.resume = 0;
                return st.ec;
            }
        }

        // VFALCO not sure how to handle path on resume
        auto const saved = path;
        // VFALCO TODO adjust path
        ec = e.handler->operator()(req, res, st);
        if(! ec.failed())
            return ec;
        if(ec == error::detach)
        {
            if( st.resume == 0)
                st.resume = st.pos;
            return ec;
        }
        if(ec == error::close)
            return ec;
        if( ec != error::next)
            goto do_error;
        path = saved;
    }
    return error::next;

do_error:
    for(auto it = impl_->errfns.begin();
        it != impl_->errfns.end(); ++it)
    {
        ec = (*it)->operator()(req, res, ec);
        if(! ec.failed())
            return {};
        if( ec == error::close ||
            ec == error::detach)
        {
            // VFALCO Disallow these for now
            detail::throw_invalid_argument();
        }
    }
    return ec;
}

auto
any_router::
resume(
    void* req, void* res, route_state& st,
    system::error_code const& ec) const ->
        system::error_code
{
    st.ec = ec;
    return invoke(req, res, st);
}

void
any_router::
append(
    bool prefix,
    http_proto::method method,
    core::string_view pat,
    handler_ptr h)
{
    // delete the last entry if it is empty, to handle the case where
    // the user calls route() without actually adding anything after.
    if( ! impl_->list.empty() &&
        ! impl_->list.back().handler)
    {
        --impl_->size;
        impl_->list.pop_back();
    }

    impl_->list.emplace_back(prefix, method, pat, std::move(h));
    ++impl_->size;
}

void
any_router::
append_err(errfn_ptr h)
{
    impl_->errfns.emplace_back(std::move(h));
}

//------------------------------------------------

#if 0
static bool match(
    urls::segments_encoded_view path,
    path_rule_t::value_type const& pat)
{
    auto it0 = path.begin();
    auto it1 = pat.v.begin();
    auto const end0 = path.end();
    auto const end1 = pat.v.end();

    while(it0 != end0 && it1 != end1)
    {
        auto const& seg0 = *it0++;
        auto const& seg1 = *it1++;
        if(*seg0 != seg1.s)
            return false;
    }

    return it0 == end0 && it1 == end1;
}
#endif

} // detail
} // beast2
} // boost
