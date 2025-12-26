//
// Copyright (c) 2025 Amlal El Mahrouss (amlal at nekernel dot org)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/server/serve_not_found.hpp>
#include <boost/capy/file.hpp>
#include <boost/http_proto/file_source.hpp>
#include <boost/http_proto/status.hpp>
#include <boost/http_proto/string_body.hpp>
#include <sstream>

namespace boost {
namespace beast2 {

    struct serve_not_found::serve_impl
    {
        std::string path_;
    };

    serve_not_found::serve_not_found(const core::string_view& path)
        : impl_(new serve_impl())
    {
        impl_->path_ = path;
    }

    serve_not_found::~serve_not_found()
    {
        if (impl_)
            delete impl_;

        impl_ = nullptr;
    }

    serve_not_found& serve_not_found::operator=(serve_not_found&& other) noexcept
    {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;

        return *this;
    }

    serve_not_found::
    serve_not_found(serve_not_found&& other) noexcept
        : impl_(other.impl_)
    {
        other.impl_ = nullptr;
    }

    http::route_result
    serve_not_found::operator()(
        http::route_params& p) const
    {
        std::string path{impl_->path_};
        path += p.path;

        system::error_code ec{};

        capy::file f;

        f.open(path.c_str(), capy::file_mode::scan, ec);

        if (!ec.failed())
            return http::route::next;

        p.res.set_start_line(http::status::not_found);
        p.res.append(http::field::content_type, "text/html");
        
        std::string body;
        std::ostringstream ss;
        
        ss <<
            "<HTML>"
            "<HEAD>"
            "<TITLE>NOT FOUND</TITLE>"
            "</HEAD>\n"
            "<BODY>"
            "<H1>NOT FOUND</H1>"
            "<P>THE FOLLOWING RESOURCE: " << p.path << ", WAS NOT FOUND.</P>"
            "</BODY>"
            "</HTML>"
            ;

        body = ss.str();

        // send 404 template to client
        p.serializer.start(p.res,
            http::string_body( std::move(body)));

        return http::route::send;
    }

}
}