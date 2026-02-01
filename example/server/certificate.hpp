//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_EXAMPLE_SERVER_CERTIFICATE_HPP
#define BOOST_BEAST2_EXAMPLE_SERVER_CERTIFICATE_HPP

#include <boost/corosio/tls_context.hpp>
#include <cstddef>
#include <memory>

namespace boost {
namespace beast2 {

/*  Load a signed certificate into the TLS context, and configure
    the context for use with a server.

    For this to work with the browser or operating system, it is
    necessary to import the certificate in the file "beast-test-CA.crt",
    which accompanies the library, into  the local certificate store,
    browser, or operating system depending on your environment.
*/
void
load_server_certificate(
    corosio::tls_context ctx);

} // beast2
} // boost

#endif
