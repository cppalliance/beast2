//
// Copyright (c) 2016-2019 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_ENDPOINT_HPP
#define BOOST_BEAST2_ENDPOINT_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/url/ipv4_address.hpp>
#include <boost/url/ipv6_address.hpp>
#include <boost/url/host_type.hpp>
#include <iosfwd>

namespace boost {
namespace beast2 {

/** Represents a host and port

    This class represents a network endpoint
    consisting of a host and port. The host may
    be specified as a domain name, an IPv4 address,
    or an IPv6 address.
*/
class endpoint
{
public:
    BOOST_BEAST2_DECL
    ~endpoint();

    endpoint() = default;

    BOOST_BEAST2_DECL
    endpoint(
        endpoint const& other) noexcept;

    BOOST_BEAST2_DECL
    endpoint(
        urls::ipv4_address const& addr,
        unsigned short port) noexcept;

    BOOST_BEAST2_DECL
    endpoint(
        urls::ipv6_address const& addr,
        unsigned short port) noexcept;

    BOOST_BEAST2_DECL
    endpoint& operator=(endpoint const&) noexcept;

    unsigned short
    port() const noexcept
    {
        return port_;
    }

    urls::host_type
    kind() const noexcept
    {
        return kind_;
    }

    bool is_ipv4() const noexcept
    {
        return kind_ == urls::host_type::ipv4;
    }

    bool is_ipv6() const noexcept
    {
        return kind_ == urls::host_type::ipv6;
    }

    urls::ipv4_address const&
    get_ipv4() const noexcept
    {
        BOOST_ASSERT(kind_ == urls::host_type::ipv4);
        return ipv4_;
    }

    urls::ipv6_address const&
    get_ipv6() const noexcept
    {
        BOOST_ASSERT(kind_ == urls::host_type::ipv6);
        return ipv6_;
    }

    friend
    std::ostream&
    operator<<(
        std::ostream& os,
        endpoint const& ep)
    {
        ep.format(os);
        return os;
    }

private:
    BOOST_BEAST2_DECL
    void format(std::ostream&) const;

    urls::host_type kind_ = urls::host_type::none;
    unsigned short port_ = 0;
    union
    {
        urls::ipv4_address ipv4_;
        urls::ipv6_address ipv6_;
    };

};

} // beast2
} // boost

#endif
