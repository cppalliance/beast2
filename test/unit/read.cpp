//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/http_io
//

// Test that header file is self-contained.
#include <boost/http_io/read.hpp>

#include "test_suite.hpp"

namespace boost {
namespace http_io {

#if 0

auto read_some( Stream&, parser& );
auto read_some( Stream&, parser&, DynamicBuffer& );
auto read( Stream&, parser& );
auto read( Stream&, parser&, DynamicBuffer& );


    //--------------------------------------------

    read( s, p );           // read message

    p.header();             // header
    p.body();               // decoded body

    //--------------------------------------------

    read_some( s, p );      // read header
    if( ! p.is_complete() )
        read( s, p );       // read body

    p.header();             // header
    p.body();               // decoded body

    //--------------------------------------------

    read_some( s, p );      // read header
    read( s, p, b );        // read body into b

    p.header();             // header
    b;                      // decoded body

    //--------------------------------------------

    read_some( s, p, b );   // read header, some body
    if( ! p.is_complete() )
        read( s, p, b );    // read body into b
    else
        // (avoid immediate completion)

    p.header();             // header
    b;                      // decoded body

    //--------------------------------------------

    read_some( s, p );      // read header
    if( ! p.is_complete() )
        read( s, p, b );    // read body into b
    else if( ! p.body().empty() )
        p.append_body( b ); // not an I/O

    p.header();             // header
    b;                      // decoded body

    //--------------------------------------------

    read( s, p, ec );       // read header, some body
    if( ec == error::buffer_full )
        ec = {};
    if( ! ec.failed() )
    {
        process( p,body() );
        p.discard_body();
    }

#endif

class read_test
{
public:
    void
    testRead()
    {
    }

    void
    run()
    {
        testRead();
    }
};

TEST_SUITE(read_test, "boost.http_io.read");

} // http_io
} // boost
