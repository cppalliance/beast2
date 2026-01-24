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
#include <boost/capy/ex/system_context.hpp>

namespace boost {
namespace beast2 {

namespace {

class log_service_impl
    : public log_service
    , public capy::execution_context::service
{
public:
    using key_type = log_service;

    explicit
    log_service_impl(capy::execution_context&) noexcept
    {
    }

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

    void shutdown() override {}

private:
    log_sections ls_;
};

} // (anon)

log_service&
use_log_service()
{
    return capy::get_system_context().use_service<log_service_impl>();
}

} // beast2
} // boost
