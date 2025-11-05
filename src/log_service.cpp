//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/log_service.hpp>
#include <boost/beast2/logger.hpp>

namespace boost {
namespace beast2 {

namespace {

class log_service_impl
    : public log_service
{
public:
    using key_type = log_service;

    section
    get_section(
        core::string_view name) override
    {
        return ls_.get(name);
    }

    auto
    get_sections() const noexcept ->
        std::vector<section> override
    {
        return ls_.get_sections();
    }

private:
    log_sections ls_;
};

} // (anon)

log_service&
use_log_service(
    polystore& ps)
{
    return ps.use<log_service_impl>();
}

} // beast2
} // boost
