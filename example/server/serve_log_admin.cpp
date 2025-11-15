//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include "serve_log_admin.hpp"
#include <boost/beast2/error.hpp>
#include <boost/beast2/format.hpp>
#include <boost/beast2/log_service.hpp>

namespace boost {
namespace beast2 {

namespace {

/** Handler to serve a log admin page
*/
class serve_log_page
{
public:
    serve_log_page(
        polystore& ps)
        : ls_(use_log_service(ps))
    {
    }

    system::error_code
    operator()(
        Request&,
        Response& res) const
    {
        auto const v = ls_.get_sections();
        std::string s;
        format_to(s, "<html>\n");
        format_to(s, "<head>\n");
        format_to(s, "<style>\n");
        format_to(s, "table, th, td {\n");
        format_to(s, "  border: 1px solid black;\n");
        format_to(s, "  border-collapse: collapse;\n");
        format_to(s, "}\n");
        format_to(s, "</style>\n");
        format_to(s, "</head>\n");
        format_to(s, "<body>\n");
        format_to(s, "<h1>Log Configuration</h1>\n");
        format_to(s, "<table>\n");
        format_to(s, "<tr><th style=\"min-width:100px\">Name</th><th>Level</th></tr>\n");
        for(auto const& sect : v)
        {
            auto const sel =
            [&](int level) -> core::string_view
            {
                return sect.threshold() == level ? " selected" : "";
            };
            format_to(s, "<tr><td>{}</td><td>", sect.name());
            format_to(s, "    <form action=\"submit\" method=\"GET\">\n");
            format_to(s, "    <input type=\"hidden\" name=\"name\", value=\"{}\">\n", sect.name());
            format_to(s, "    <select name=\"level\" onchange=\"this.form.submit()\">\n");
            format_to(s, "        <option value=\"{}\"{}>{}: Trace</option>\n", 0, sel(0), 0);
            format_to(s, "        <option value=\"{}\"{}>{}: Debug</option>\n", 1, sel(1), 1);
            format_to(s, "        <option value=\"{}\"{}>{}: Info</option>\n" , 2, sel(2), 2);
            format_to(s, "        <option value=\"{}\"{}>{}: Warn</option>\n" , 3, sel(3), 3);
            format_to(s, "        <option value=\"{}\"{}>{}: Fatal</option>\n", 4, sel(4), 4);
            format_to(s, "    </select>\n");
            format_to(s, "    </form>\n");
        }
        format_to(s, "</td></tr>\n");
        format_to(s, "</table>\n");
        format_to(s, "</body>\n");
        format_to(s, "</html>\n");

        res.status(http_proto::status::ok);
        res.m.set(http_proto::field::content_type, "text/html; charset=UTF-8");
        res.set_body(std::move(s));
        return error::success;
    }

private:
    log_service& ls_;
};

//------------------------------------------------

/** Handler to receive a form submit
*/
class handle_submit
{
public:
    handle_submit(
        polystore& ps)
        : ls_(use_log_service(ps))
    {
    }

    system::error_code
    operator()(
        Request&,
        Response& res) const
    {
        res.status(http_proto::status::ok);
        res.m.set(http_proto::field::content_type, "plain/text; charset=UTF-8");
        res.set_body("submit");
        return error::success;
    }

private:
    log_service& ls_;
};

} // (anon)

//------------------------------------------------

router
serve_log_admin(
    polystore& ps)
{
    router r;
    r.add(http_proto::method::get, "/", serve_log_page(ps));
    r.add(http_proto::method::get, "/submit", handle_submit(ps));
    return r;
}

} // beast2
} // boost
