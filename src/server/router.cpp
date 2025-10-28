//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include "src/server/route_rule.hpp"
#include <boost/beast2/server/router.hpp>
#include <boost/url/decode_view.hpp>
#include <map>
#include <string>
#include <vector>

namespace boost {
namespace beast2 {

router_base::any_handler::~any_handler() = default;

//------------------------------------------------

struct router_base::entry
{
    http_proto::method method;
    // VFALCO For now we do exact prefix-match
    std::string exact_prefix;
    path_rule_t::value_type pat;
    std::vector<handler_ptr> handlers;
};

struct router_base::impl
{
    http_proto::method(*get_method)(void*);
    urls::segments_encoded_view&(*get_path)(void*);
    std::vector<handler_ptr> v0;
    std::vector<entry> patterns;
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
    std::vector<handler_ptr> handlers;
    handlers.emplace_back(std::move(h));

    impl_->patterns.emplace_back(entry{
        method,
        path, // exact_prefix
        grammar::parse(it, end, path_rule).value(),
        std::move(handlers)});
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

bool
router_base::
invoke(
    void* req, void* res) const
{
    // global handlers
    for(auto const& r : impl_->v0)
    {
        if(r->operator()(req, res))
            return true;
    }

    auto method = impl_->get_method(req);
    auto& path = impl_->get_path(req);
    std::string path_str = std::string(path.buffer());
    for(auto const& r : impl_->patterns)
    {
        if(r.method != method &&
            method != http_proto::method::unknown)
            continue;
        //if(match(path, r.pat))
        // VFALCO exact-prefix matching for now
        if( core::string_view(path_str).starts_with(
            core::string_view(r.exact_prefix)))
        {
            // matched
            for(auto& e : r.handlers)
            {
                if(e->operator()(req, res))
                    return true;
            }

            // no handler indicated it handled the request
            return false;
        }
    }

    // no route matched
    return false;
}

} // beast2
} // boost
