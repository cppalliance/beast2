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

#include <boost/http_proto/request_base.hpp>
#include <boost/http_proto/response.hpp>
#include <boost/http_proto/serializer.hpp>
#include <boost/core/detail/string_view.hpp>

namespace boost {

struct work_params
{
    http_proto::request_base const& req;
    http_proto::response& res;
    http_proto::serializer& sr;
    bool is_shutting_down;
};

void
handle_request(
    core::string_view doc_root,
    work_params const& params);

void
handle_https_redirect(
    work_params const& params);

//------------------------------------------------

/** A handler which serves files from a specified root path
*/
class file_work_handler
{
public:
    explicit
    file_work_handler(
        core::string_view doc_root)
        : doc_root_(doc_root)
    {
    }

    template<class AsyncStream>
    void on_request_header(
        AsyncStream& stream,
        work_params const& params) const
    {
    }

    template<class AsyncStream>
    void on_request(
        AsyncStream& stream,
        work_params const& params) const
    {
        return on_request(params);
    }

    void on_request(
        work_params const& params) const;

private:
    core::string_view doc_root_;
};

//------------------------------------------------

} // boost

#endif
