//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_EXAMPLE_SERVER_HANDLER_HPP
#define BOOST_HTTP_IO_EXAMPLE_SERVER_HANDLER_HPP

#include <boost/http_io/detail/config.hpp>
#include <boost/http_io/server/router.hpp>
#include <boost/http_proto/request_base.hpp>
#include <boost/http_proto/response.hpp>
#include <boost/http_proto/serializer.hpp>
#include <boost/core/detail/string_view.hpp>

namespace boost {
namespace http_io {

//------------------------------------------------

struct work_params
{
    http_proto::request_base const& req;
    http_proto::response& res;
    http_proto::serializer& sr;
    bool is_shutting_down;
};

struct responder
{
    virtual ~responder();
    virtual void on_complete(
        work_params const& params) = 0;
};

using router_type = router<responder>;

//------------------------------------------------

class https_redirect_responder : public router_type::factory
{
public:
    void operator()(router_type::handler& dest) const override;

private:
    class handler;
};

//------------------------------------------------

class file_responder : public router_type::factory
{
public:
    file_responder(
        core::string_view doc_root)
        : doc_root_(doc_root)
    {
    }

    void operator()(router_type::handler& dest) const override;

private:
    class handler;

    std::string doc_root_;
};

} // http_io
} // boost

#endif
