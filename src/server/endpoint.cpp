//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include <boost/http_io/endpoint.hpp>
#include <ostream>

namespace boost {
namespace http_io {

endpoint::
~endpoint()
{
    switch(kind_)
    {
    case urls::host_type::ipv4:
        ipv4_.~ipv4_address();
        break;
    case urls::host_type::ipv6:
        ipv6_.~ipv6_address();
        break;
    default:
        break;
    }
}

endpoint::
endpoint(
    endpoint const& other) noexcept
    : kind_(other.kind_)
    , port_(other.port_)
{
    switch(kind_)
    {
    case urls::host_type::ipv4:
        new(&ipv4_) urls::ipv4_address(other.ipv4_);
        break;
    case urls::host_type::ipv6:
        new(&ipv6_) urls::ipv6_address(other.ipv6_);
        break;
    default:
        break;
    }
}

endpoint::
endpoint(
    urls::ipv4_address const& addr,
    unsigned short port) noexcept
    : kind_(urls::host_type::ipv4)
    , port_(port)
{
    new(&ipv4_) urls::ipv4_address(addr);
}

endpoint::
endpoint(
    urls::ipv6_address const& addr,
    unsigned short port) noexcept
    : kind_(urls::host_type::ipv6)
    , port_(port)
{
    new(&ipv6_) urls::ipv6_address(addr);
}

endpoint&
endpoint::
operator=(endpoint const& other) noexcept
{
    if(this == &other)
        return *this;
    this->~endpoint();
    return *new(this) endpoint(other);
}

void
endpoint::
format(std::ostream& os) const
{
    switch(kind_)
    {
    case urls::host_type::ipv4:
        os << ipv4_;
        break;
    case urls::host_type::ipv6:
        os << ipv6_;
        break;
    default:
        os << "none";
        break;
    }
    if(port_)
        os << ':' << port_;
}

} // http_io
} // boost
