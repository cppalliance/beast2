//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include "post_work.hpp"

namespace boost {
namespace beast2 {

/*
struct etag
{
    Request& req;
    Response& res;
    sha1_state digest;

    void operator()( resumer resume )
    {
        char buf[8192];
        system::error_code ec;
        auto nread = res.body.read(
            buffers::make_buffer(buf), ec);
        digest.update( buf, nread );
        if(ec == error::eof)
        {
            res.body.rewind();
            res.set_header(
                http::field::etag,
                to_hex(digest.finalize()) );
            return resume( route::next );
        }
        if( ec.failed() )
            return resume( ec );
        // we will get called again
    }
};
*/

namespace {

struct task
{
    std::size_t i = 10;

    void
    operator()(http_proto::resumer resume)
    {
        if(i--)
            return;
        resume(http_proto::route::next);
    }
};

} // (anon)

//------------------------------------------------

http_proto::route_result
post_work::
operator()(
    http_proto::route_params& p) const
{
    return p.post(task());
}

} // beast2
} // boost
