//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include "any_iostream.hpp"
#include "any_stream.hpp"
#include "base64.hpp"
#include "connect.hpp"
#include "cookie.hpp"
#include "file.hpp"
#include "message.hpp"
#include "mime_type.hpp"
#include "multipart_form.hpp"
#include "utils.hpp"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/buffers.hpp>
#include <boost/http_io.hpp>
#include <boost/http_proto.hpp>
#include <boost/url/encode.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/rfc/pchars.hpp>
#include <boost/url/url.hpp>

#include <cstdlib>

namespace ch       = std::chrono;
namespace http_io  = boost::http_io;
using system_error = boost::system::system_error;

#ifdef BOOST_HTTP_PROTO_HAS_ZLIB
inline const bool http_proto_has_zlib = true;
#else
inline const bool http_proto_has_zlib = false;
#endif

void
set_target(
    const po::variables_map& vm,
    http_proto::request& request,
    const urls::url_view& url)
{
    if(vm.count("request-target"))
    {
        request.set_target(
            vm.at("request-target").as<std::string>());
        return;
    }

    if(url.encoded_target().empty())
    {
        request.set_target("/");
        return;
    }

    request.set_target(url.encoded_target());
}

struct is_redirect_result
{
    bool is_redirect = false;
    bool need_method_change = false;
};

is_redirect_result
is_redirect(
    const po::variables_map& vm,
    http_proto::status status) noexcept
{
    // The specifications do not intend for 301 and 302
    // redirects to change the HTTP method, but most
    // user agents do change the method in practice.
    switch(status)
    {
    case http_proto::status::moved_permanently:
        return { true, !vm.count("post301") };
    case http_proto::status::found:
        return { true, !vm.count("post302") };
    case http_proto::status::see_other:
        return { true, !vm.count("post303") };
    case http_proto::status::temporary_redirect:
    case http_proto::status::permanent_redirect:
        return { true, false };
    default:
        return { false, false };
    }
}

bool
can_reuse_connection(
    http_proto::response_view response,
    const urls::url_view& a,
    const urls::url_view& b) noexcept
{
    if(a.encoded_origin() != b.encoded_origin())
        return false;

    if(response.version() != http_proto::version::http_1_1)
        return false;

    if(response.metadata().connection.close)
        return false;

    return true;
}

http_proto::request
create_request(
    const po::variables_map& vm,
    const message& msg,
    const urls::url_view& url)
{
    using field   = http_proto::field;
    using method  = http_proto::method;
    using version = http_proto::version;

    auto request = http_proto::request{};

    request.set_method(vm.count("head") ? method::head : method::get);

    if(vm.count("request"))
        request.set_method(vm.at("request").as<std::string>());

    request.set_version(
        vm.count("http1.0") ? version::http_1_0 : version::http_1_1);

    set_target(vm, request, url);

    request.set(field::accept, "*/*");
    request.set(field::host, url.authority().encoded_host_and_port().decode());

    msg.set_headers(request);

    if(vm.count("continue-at"))
    {
        auto value = "bytes=" +
            std::to_string(vm.at("continue-at").as<std::uint64_t>()) + "-";
        request.set(field::range, value);
    }

    if(vm.count("range"))
        request.set(field::range, "bytes=" + vm.at("range").as<std::string>());

    if(vm.count("user-agent"))
    {
        request.set(field::user_agent, vm.at("user-agent").as<std::string>());
    }
    else
    {
        request.set(field::user_agent, "Boost.Http.Io");
    }

    if(vm.count("referer"))
        request.set(field::referer, vm.at("referer").as<std::string>());

    auto credentials = [&]()
    {
        if(vm.count("user"))
            return vm.at("user").as<std::string>();
        return url.userinfo();
    }();

    if(!credentials.empty())
    {
        auto basic_auth = std::string{ "Basic " };
        base64_encode(basic_auth, credentials);
        request.set(field::authorization, basic_auth);
    }

    if(vm.count("compressed") && http_proto_has_zlib)
        request.set(field::accept_encoding, "gzip, deflate");

    // Set user provided headers
    if(vm.count("header"))
    {
        for(core::string_view header : vm.at("header").as<std::vector<std::string>>())
        {
            if(auto pos = header.find(':'); pos != std::string::npos)
            {
                auto name  = header.substr(0, pos);
                auto value = header.substr(pos + 1);
                if(!value.empty())
                    request.set(name, value);
                else
                    request.erase(name);
            }
            else if(auto pos = header.find(';'); pos != std::string::npos)
            {
                auto name = header.substr(0, pos);
                request.set(name, "");
            }
        }
    }

    return request;
}

asio::awaitable<void>
request(
    const po::variables_map& vm,
    any_ostream& body_output,
    std::optional<any_ostream>& header_output,
    const std::set<urls::scheme>& allowed_redirect_protos,
    std::uint32_t max_redirects,
    bool show_headers,
    message& msg,
    std::optional<cookie_jar>& cookie_jar,
    core::string_view explicit_cookies,
    ssl::context& ssl_ctx,
    http_proto::context& http_proto_ctx,
    http_proto::request request,
    const urls::url_view& url)
{
    using field     = http_proto::field;
    auto executor   = co_await asio::this_coro::executor;
    auto parser     = http_proto::response_parser{ http_proto_ctx };
    auto serializer = http_proto::serializer{ http_proto_ctx };

    auto connect_to = [&](const urls::url_view& url)
    {
        auto timeout = vm.count("connect-timeout")
            ? ch::duration_cast<ch::steady_clock::duration>(
                ch::duration<float>(vm.at("connect-timeout").as<float>()))
            : ch::steady_clock::duration::max();

        return asio::co_spawn(
            executor,
            connect(vm, ssl_ctx, http_proto_ctx, url),
            asio::cancel_after(timeout));
    };

    auto set_cookies = [&](const urls::url_view& url, bool trusted)
    {
        auto field = cookie_jar ? cookie_jar->make_field(url) : std::string{};
        
        if(trusted)
            field.append(explicit_cookies);

        request.erase(field::cookie);

        if(!field.empty())
            request.set(field::cookie, field);
    };

    auto extract_cookies = [&](const urls::url_view& url)
    {
        if(!cookie_jar)
            return;

        for(auto sv : parser.get().find_all(field::set_cookie))
            cookie_jar->add(url, parse_cookie(sv).value());
    };

    auto stream_headers = [&]()
    {
        if(show_headers)
            body_output << parser.get().buffer();

        if(header_output.has_value())
            header_output.value() << parser.get().buffer();
    };

    auto stream = co_await connect_to(url);
    parser.reset();

    auto send_request_and_read_headers =
        [&](const urls::url_view& url, bool trusted) -> asio::awaitable<void>
    {
        set_cookies(url, trusted);
        msg.start_serializer(serializer, request);
        auto [ec, _] = co_await http_io::async_write(stream, serializer, asio::as_tuple);
        if(ec)
        {
            if(ec == http_proto::error::expect_100_continue)
            {
                auto expect100_timeout = vm.count("expect100-timeout")
                ? ch::duration_cast<ch::steady_clock::duration>(
                    ch::duration<float>(vm.at("expect100-timeout").as<float>()))
                : ch::steady_clock::duration{ch::seconds{ 1 }};

                parser.start();
                auto [ec, _] = co_await http_io::async_read_header(
                    stream,
                    parser,
                    asio::cancel_after(expect100_timeout, asio::as_tuple));
                if(ec)
                {
                    if(ec != asio::error::operation_aborted)
                        throw system_error{ ec };
                }
                else
                {
                    extract_cookies(url);
                    stream_headers();
                    if(parser.get().status() != http_proto::status::continue_)
                        co_return;
                    parser.start();
                }

                co_await http_io::async_write(stream, serializer, asio::as_tuple);
            }
            else
            {
                throw system_error{ ec };
            }
        }
        else
        {
            parser.start();
        }

        co_await http_io::async_read_header(stream, parser);
        extract_cookies(url);
        stream_headers();

        // expect100-timeout
        if(parser.get().status() == http_proto::status::continue_)
        {
            parser.start();
            co_await http_io::async_read_header(stream, parser);
            extract_cookies(url);
            stream_headers();
        }
    };

    co_await send_request_and_read_headers(url, true);

    // handle redirects
    auto referer = urls::url{ url };
    for(;;)
    {
        auto [is_redirect, need_method_change] =
            ::is_redirect(vm, parser.get().status());

        if(!is_redirect ||
           (!vm.count("location") && !vm.count("location-trusted")))
            break;

        if(max_redirects-- == 0)
            throw std::runtime_error{ "Maximum redirects followed" };

        auto response = parser.get();
        if(auto it = response.find(field::location);
           it != response.end())
        {
            auto location = urls::url{};

            urls::resolve(
                referer,
                urls::parse_uri_reference(it->value).value(),
                location);

            if(!allowed_redirect_protos.contains(location.scheme_id()))
                throw std::runtime_error{
                    "Protocol "
                    + std::string{ location.scheme() } +
                    " not supported or disabled" };

            if(can_reuse_connection(response, referer, location))
            {
                // Discard the body
                // TODO: drop the connection if body is large
                if(request.method() != http_proto::method::head)
                {
                    while(!parser.is_complete())
                    {
                        parser.consume_body(
                            buffers::buffer_size(parser.pull_body()));
                        co_await http_io::async_read_some(stream, parser);
                    }
                }
                else
                {
                    parser.reset();
                }
            }
            else
            {
                // clean shutdown
                if(!vm.count("proxy"))
                    co_await stream.async_shutdown(
                        asio::cancel_after(
                            ch::milliseconds{ 500 }, asio::as_tuple));

                stream = co_await connect_to(location);
                parser.reset();
            }

            // Change the method according to RFC 9110, Section 15.4.4.
            if(need_method_change && !vm.count("head"))
            {
                request.set_method(http_proto::method::get);
                request.erase(field::content_length);
                request.erase(field::content_encoding);
                request.erase(field::content_type);
                request.erase(field::expect);
                msg = {}; // drop the body
            }

            set_target(vm, request, location);

            const bool trusted =
                (url.encoded_origin() == location.encoded_origin()) ||
                vm.count("location-trusted");

            if(!trusted)
                request.erase(field::authorization);

            request.set(field::host,
                location.authority().encoded_host_and_port().decode());
            referer.remove_userinfo();
            request.set(field::referer, referer);

            referer = location;

            co_await send_request_and_read_headers(location, trusted);
        }
        else
        {
            throw std::runtime_error{ "Bad redirect response" };
        }
    }

    if(vm.count("fail") && parser.get().status_int() >= 400)
        throw std::runtime_error{
            "The requested URL returned error: " +
            std::to_string(parser.get().status_int()) };

    if(vm.count("remote-header-name"))
    {
        for(auto sv : parser.get().find_all(field::content_disposition))
        {
            auto filepath = extract_filename_form_content_disposition(sv);
            if(filepath.has_value())
            {
                // stripp off the potential path
                auto filename = ::filename(filepath.value());
                if(filename.empty())
                    continue;

                fs::path path = vm.count("output-dir")
                    ? vm.at("output-dir").as<std::string>()
                    : "";
                path.append(filename.begin(), filename.end());
                body_output = any_ostream{ path };
            }
        }
    }

    // stream body
    if(request.method() != http_proto::method::head)
    {
        for(;;)
        {
            for(auto cb : parser.pull_body())
            {
                auto chunk = core::string_view{
                    static_cast<const char*>(cb.data()), cb.size() };

                if(body_output.is_tty() &&
                    chunk.find(char{0}) != core::string_view::npos)
                {
                    std::cerr <<
                        "Warning: Binary output can mess up your terminal.\n"
                        "Warning: Use \"--output -\" to tell burl to output it to your terminal anyway, or\n"
                        "Warning: consider \"--output <FILE>\" to save to a file.\n";
                    co_return;
                }

                body_output << chunk;
                parser.consume_body(cb.size());
            }

            if(parser.is_complete())
                break;

            auto [ec, _] =
                co_await http_io::async_read_some(stream, parser, asio::as_tuple);
            if(ec && ec != http_proto::condition::need_more_input)
                throw system_error{ ec };
        }
    }

    // clean shutdown
    if(!vm.count("proxy"))
        co_await stream.async_shutdown(
            asio::cancel_after(
                ch::milliseconds{ 500 }, asio::as_tuple));
    
    if(vm.count("fail-with-body") && parser.get().status_int() >= 400)
        throw std::runtime_error{
            "The requested URL returned error: " +
            std::to_string(parser.get().status_int()) };
};

boost::optional<std::string>
parse_data_options(const po::variables_map& vm)
{
    boost::optional<std::string> rs;

    auto append_striped = [&](std::istream& input)
    {
        char ch;
        while(input.get(ch))
        {
            if(ch != '\r' && ch != '\n' && ch != '\0')
                rs->push_back(ch);
        }
    };

    auto append_encoded = [&](core::string_view sv)
    {
        urls::encoding_opts opt;
        opt.space_as_plus = true;
        urls::encode(sv, urls::pchars, opt, urls::string_token::append_to(*rs));
    };

    auto init = [&, init = false]() mutable
    {
        if(std::exchange(init, true))
            rs->push_back('&');
        else
            rs.emplace();
    };

    if(vm.count("data"))
    {
        for(core::string_view sv : vm.at("data").as<std::vector<std::string>>())
        {
            init();

            if(sv.starts_with('@'))
                append_striped(any_istream{ sv.substr(1) });
            else
                rs->append(sv);
        }
    }

    if(vm.count("data-ascii"))
    {
        for(core::string_view sv :
            vm.at("data-ascii").as<std::vector<std::string>>())
        {
            init();

            if(sv.starts_with('@'))
                append_striped(any_istream{ sv.substr(1) });
            else
                rs->append(sv);
        }
    }

    if(vm.count("data-binary"))
    {
        for(core::string_view sv :
            vm.at("data-binary").as<std::vector<std::string>>())
        {
            init();

            if(sv.starts_with('@'))
                any_istream{ sv.substr(1) }.append_to(*rs);
            else
                rs->append(sv);
        }
    }

    if(vm.count("data-raw"))
    {
        for(core::string_view sv :
            vm.at("data-raw").as<std::vector<std::string>>())
        {
            init();

            rs->append(sv);
        }
    }

    if(vm.count("data-urlencode"))
    {
        for(core::string_view sv :
            vm.at("data-urlencode").as<std::vector<std::string>>())
        {
            init();

            if(auto pos = sv.find('='); pos != sv.npos)
            {
                auto name    = sv.substr(0, pos);
                auto content = sv.substr(pos + 1);
                if(!name.empty())
                {
                    rs->append(name);
                    rs->push_back('=');
                }
                append_encoded(content);
            }
            else if(auto pos = sv.find('@'); pos != sv.npos)
            {
                auto name     = sv.substr(0, pos);
                auto filename = sv.substr(pos + 1);
                if(!name.empty())
                {
                    rs->append(name);
                    rs->push_back('=');
                }
                std::string tmp;
                any_istream{ filename }.append_to(tmp);
                append_encoded(tmp);
            }
            else
            {
                append_encoded(sv);
            }
        }
    }

    return rs;
}

int
main(int argc, char* argv[])
{
    try
    {
        auto odesc = po::options_description{"Options"};
        odesc.add_options()
            ("abstract-unix-socket",
                po::value<std::string>()->value_name("<path>"),
                "Connect via abstract Unix domain socket")
            ("cacert",
                po::value<std::string>()->value_name("<file>"),
                "CA certificate to verify peer against")
            ("capath",
                po::value<std::string>()->value_name("<dir>"),
                "CA directory to verify peer against")
            ("cert,E",
                po::value<std::string>()->value_name("<certificate>"),
                "Client certificate file")
            ("ciphers",
                po::value<std::string>()->value_name("<list>"),
                "SSL ciphers to use")
            ("compressed", "Request compressed response")
            ("connect-timeout",
                po::value<float>()->value_name("<frac sec>"),
                "Maximum time allowed for connection")
            ("connect-to",
                po::value<std::vector<std::string>>()->value_name("<H1:P1:H2:P2>"),
                "Connect to host")
            ("continue-at,C",
                po::value<std::uint64_t>()->value_name("<offset>"),
                "Resume transfer offset")
            ("cookie,b",
                po::value<std::vector<std::string>>()->value_name("<data|filename>"),
                "Send cookies from string/file")
            ("cookie-jar,c",
                po::value<std::string>()->value_name("<filename>"),
                "Write cookies to <filename> after operation")
            ("create-dirs", "Create necessary local directory hierarchy")
            ("curves",
                po::value<std::string>()->value_name("<list>"),
                "(EC) TLS key exchange algorithm(s) to request")
            ("data,d",
                po::value<std::vector<std::string>>()->value_name("<data>"),
                "HTTP POST data")
            ("data-ascii",
                po::value<std::vector<std::string>>()->value_name("<data>"),
                "HTTP POST ASCII data")
            ("data-binary",
                po::value<std::vector<std::string>>()->value_name("<data>"),
                "HTTP POST binary data")
            ("data-raw",
                po::value<std::vector<std::string>>()->value_name("<data>"),
                "HTTP POST data, '@' allowed")
            ("data-urlencode",
                po::value<std::vector<std::string>>()->value_name("<data>"),
                "HTTP POST data URL encoded")
            ("disallow-username-in-url", "Disallow username in URL")
            ("dump-header,D",
                po::value<std::string>()->value_name("<filename>"),
                "Write the received headers to <filename>")
            ("expect100-timeout",
                po::value<float>()->value_name("<frac sec>"),
                "How long to wait for 100-continue")
            ("fail,f", "Fail fast with no output on HTTP errors")
            ("fail-with-body", "Fail on HTTP errors but save the body")
            ("form,F",
                po::value<std::vector<std::string>>()->value_name("<name=content>"),
                "Specify multipart MIME data")
            ("form-string",
                po::value<std::vector<std::string>>()->value_name("<name=string>"),
                "Specify multipart MIME data")
            ("key",
                po::value<std::string>()->value_name("<key>"),
                "Private key file")
            ("get,G", "Put the post data in the URL and use GET")
            ("head,I", "Show document info only")
            ("header,H",
                po::value<std::vector<std::string>>()->value_name("<header>"),
                "Pass custom header(s) to server")
            ("help,h", "produce help message")
            ("http1.0", "Use HTTP 1.0")
            ("insecure,k", "Allow insecure server connections")
            ("ipv4,4", "Resolve names to IPv4 addresses")
            ("ipv6,6", "Resolve names to IPv6 addresses")
            ("json",
                po::value<std::vector<std::string>>()->value_name("<data>"),
                "HTTP POST JSON")
            ("junk-session-cookies,j", "Ignore session cookies read from file")
            ("location,L", "Follow redirects")
            ("location-trusted", "Like --location, and send auth to other hosts")
            ("max-filesize",
                po::value<std::uint64_t>()->value_name("<bytes>"),
                "Maximum file size to download")
            ("max-redirs",
                po::value<std::int32_t>()->value_name("<num>"),
                "Maximum number of redirects allowed")
            ("max-time",
                po::value<float>()->value_name("<frac sec>"),
                "Maximum time allowed for transfer")
            ("no-keepalive", "Disable TCP keepalive on the connection")
            ("output,o",
                po::value<std::string>()->value_name("<file>"),
                "Write to file instead of stdout")
            ("output-dir",
                po::value<std::string>()->value_name("<dir>"),
                "Directory to save files in")
            ("pass",
                po::value<std::string>()->value_name("<phrase>"),
                "Passphrase for the private key")
            ("post301", "Do not switch to GET after following a 301")
            ("post302", "Do not switch to GET after following a 302")
            ("post303", "Do not switch to GET after following a 303")
            ("proto-redir",
                po::value<std::vector<std::string>>()->value_name("<protocol>"),
                "Enable/disable PROTOCOLS on redirect")
            ("proxy,x",
                po::value<std::string>()->value_name("<url>"),
                "Use this proxy")
            ("range,r",
                po::value<std::string>()->value_name("<range>"),
                "Retrieve only the bytes within range")
            ("referer,e",
                po::value<std::string>()->value_name("<url>"),
                "Referer URL")
            ("remote-header-name,J", "Use the header-provided filename")
            ("remote-name,O", "Write output to a file named as the remote file")
            ("remove-on-error", "Remove output file on errors")
            ("request,X",
                po::value<std::string>()->value_name("<method>"),
                "Specify request method to use")
            ("request-target",
                po::value<std::string>()->value_name("<path>"),
                "Specify the target for this request")
            ("resolve",
                po::value<std::vector<std::string>>()->value_name("host:port:addr"),
                "Resolve the host+port to this address")
            ("show-headers", "Show response headers in the output")
            ("tcp-nodelay", "Use the TCP_NODELAY option")
            ("tls-max",
                po::value<std::string>()->value_name("<version>"),
                "Set maximum allowed TLS version")
            ("tls13-ciphers",
                po::value<std::string>()->value_name("<list>"),
                "TLS 1.3 cipher suites to use")
            ("tlsv1.0", "Use TLSv1.0 or greater")
            ("tlsv1.1", "Use TLSv1.1 or greater")
            ("tlsv1.2", "Use TLSv1.2 or greater")
            ("tlsv1.3", "Use TLSv1.3 or greater")
            ("include,i", "Include protocol response headers in the output")
            ("unix-socket",
                po::value<std::string>()->value_name("<path>"),
                "Connect through this Unix domain socket")
            ("upload-file,T",
                po::value<std::string>()->value_name("<file>"),
                "Transfer local FILE to destination")
            ("url",
                po::value<std::string>()->value_name("<url>"),
                "URL to work with")
            ("user,u",
                po::value<std::string>()->value_name("<user:password>"),
                "Server user and password")
            ("user-agent,A",
                po::value<std::string>()->value_name("<name>"),
                "Send User-Agent <name> to server");

        auto podesc = po::positional_options_description{};
        podesc.add("url", 1);

        po::variables_map vm;
        po::store(
            po::command_line_parser{ argc, argv }
                .options(odesc)
                .positional(podesc)
                .run(),
            vm);
        po::notify(vm);

        if(vm.count("help") || !vm.count("url"))
        {
            std::cerr
                << "Usage: burl [options...] <url>\n"
                << "Example:\n"
                << "    burl https://www.example.com\n"
                << "    burl -L http://httpstat.us/301\n"
                << "    burl https://httpbin.org/post -F name=Shadi -F img=@./avatar.jpeg\n"
                << odesc;
            return EXIT_FAILURE;
        }

        urls::url url = [&]()
        {
            auto rs = urls::parse_uri(vm.at("url").as<std::string>());
            if(rs.has_error())
                throw system_error{ rs.error(), "Failed to parse URL" };
            return rs.value();
        }();

        if(vm.count("disallow-username-in-url") && url.has_userinfo())
            throw std::runtime_error{
                "Credentials was passed in the URL when prohibited" };

        if(vm.count("fail") && vm.count("fail-with-body"))
            throw std::runtime_error{
                "You must select either --fail or --fail-with-body, not both." };

        auto ioc            = asio::io_context{};
        auto ssl_ctx        = ssl::context{ ssl::context::tls_client };
        auto http_proto_ctx = http_proto::context{};

        {
            auto tls_min = 11;
            auto tls_max = 13;

            if(vm.count("tls-max"))
            {
                auto s = vm.at("tls-max").as<std::string>();
                if(     s == "1.0") tls_max = 10;
                else if(s == "1.1") tls_max = 11;
                else if(s == "1.2") tls_max = 12;
                else if(s == "1.3") tls_max = 13;
                else throw std::runtime_error{ "Wrong TLS version" };
            }

            if(vm.count("tlsv1.0")) tls_min = 10;
            if(vm.count("tlsv1.1")) tls_min = 11;
            if(vm.count("tlsv1.2")) tls_min = 12;
            if(vm.count("tlsv1.3")) tls_min = 13;

            if(tls_min > 10                ) ssl_ctx.set_options(ssl_ctx.no_tlsv1);
            if(tls_min > 11 || tls_max < 11) ssl_ctx.set_options(ssl_ctx.no_tlsv1_1);
            if(tls_min > 12 || tls_max < 12) ssl_ctx.set_options(ssl_ctx.no_tlsv1_2);
            if(tls_max < 13                ) ssl_ctx.set_options(ssl_ctx.no_tlsv1_3);
        }

        if(vm.count("insecure"))
        {
            ssl_ctx.set_verify_mode(ssl::verify_none);
        }
        else
        {
            if(vm.count("cacert"))
            {
                ssl_ctx.load_verify_file(
                    vm.at("cacert").as<std::string>());
            }
            else if(vm.count("capath"))
            {
                ssl_ctx.add_verify_path(
                    vm.at("capath").as<std::string>());
            }
            else
            {
                ssl_ctx.set_default_verify_paths();
            }

            ssl_ctx.set_verify_mode(ssl::verify_peer);
        }

        if(vm.count("cert"))
        {
            ssl_ctx.use_certificate_file(
                vm.at("cert").as<std::string>(),
                ssl::context::file_format::pem);
        }

        if(vm.count("ciphers"))
        {
            if(::SSL_CTX_set_cipher_list(
                ssl_ctx.native_handle(),
                vm.at("ciphers").as<std::string>().c_str()) != 1)
            {
                throw std::runtime_error{ "failed setting cipher list" };
            }
        }

        if(vm.count("tls13-ciphers"))
        {
            if(::SSL_CTX_set_ciphersuites(
                ssl_ctx.native_handle(),
                vm.at("tls13-ciphers").as<std::string>().c_str()) != 1)
            {
                throw std::runtime_error{ "failed setting TLS 1.3 cipher suite" };
            }
        }

        if(vm.count("curves"))
        {
            if(::SSL_CTX_set1_curves_list(
                ssl_ctx.native_handle(),
                vm.at("curves").as<std::string>().c_str()) != 1)
            {
                throw std::runtime_error{ "failed setting curves list" };
            }
        }

        if(vm.count("pass"))
        {
            ssl_ctx.set_password_callback(
                [&](auto, auto) { return vm.at("pass").as<std::string>(); });
        }

        if(vm.count("key"))
        {
            ssl_ctx.use_private_key_file(
                vm.at("key").as<std::string>(),
                ssl::context::file_format::pem);
        }

        {
            http_proto::response_parser::config cfg;

            cfg.body_limit = [&]()
            {
                if(vm.count("max-filesize"))
                    return vm.at("max-filesize").as<std::uint64_t>();
                return std::numeric_limits<std::uint64_t>::max();
            }();
            cfg.min_buffer = 1024 * 1024;
            if(http_proto_has_zlib)
            {
                cfg.apply_gzip_decoder    = true;
                cfg.apply_deflate_decoder = true;
                http_proto::zlib::install_service(http_proto_ctx);
            }
            http_proto::install_parser_service(http_proto_ctx, cfg);
        }

        auto body_output = [&]()
        {
            fs::path path;

            if(vm.count("remote-name"))
            {
                if(vm.count("output-dir"))
                    path = vm.at("output-dir").as<std::string>();

                auto segs = url.encoded_segments();
                if(segs.empty() || segs.back().empty())
                {
                    path.append("burl_response");
                }
                else
                {
                    path.append(segs.back().begin(),
                    segs.back().end());
                }
            }
            else if(vm.count("output"))
            {
                path = vm.at("output").as<std::string>();
            }
            else
            {
                return any_ostream{};
            }

            if(vm.count("create-dirs"))
                fs::create_directories(path.parent_path());

            return any_ostream{ path };
        }();

        auto header_output = [&]() -> std::optional<any_ostream>
        {
            if(vm.count("dump-header"))
                return any_ostream{
                    fs::path{ vm.at("dump-header").as<std::string>() } };
            return std::nullopt;
        }();

        auto msg = message{};

        auto data = parse_data_options(vm);

        if(data)
        {
            if(vm.count("get"))
            {
                urls::params_view params{ data.value() };
                url.encoded_params().append(params.begin(), params.end());
            }
            else
            {
                msg = string_body{ std::move(data.value()),
                    "application/x-www-form-urlencoded" };
            }
        }

        if((!!data +
            !!vm.count("form") +
            !!vm.count("json") +
            !!vm.count("upload-file")) == 2)
        {
            throw std::runtime_error{
                "You can only select one HTTP request method"};
        }

        if(vm.count("form") || vm.count("form-string"))
        {
            auto form = multipart_form{};
            if(vm.count("form"))
            {
                for(core::string_view sv :
                    vm.at("form").as<std::vector<std::string>>())
                {
                    auto [name, prefix, value, filename, type, headers] =
                        parse_form_option(sv);

                    auto is_file = false;

                    if(prefix == '@' || prefix == '<')
                    {
                        is_file = true;

                        if(!filename && prefix != '<')
                            filename = ::filename(value);

                        if(value == "-")
                        {
                            value.clear();
                            any_istream{ core::string_view{"-"} }.append_to(value);
                            is_file = false;
                        }
                        else if(!type)
                        {
                            type = mime_type(value);
                        }
                    }
                    form.append(
                        is_file,
                        std::move(name),
                        std::move(value),
                        std::move(filename),
                        std::move(type),
                        std::move(headers));
                }
            }
            if(vm.count("form-string"))
            {
                for(core::string_view sv :
                    vm.at("form-string").as<std::vector<std::string>>())
                {
                    if(auto pos = sv.find('='); pos != sv.npos)
                    {
                        form.append(
                            false,
                            sv.substr(0, pos),
                            sv.substr(pos + 1));
                    }
                    else
                    {
                        throw std::runtime_error{
                            "Illegally formatted input field"
                        };
                    }
                }
            }
            msg = std::move(form);
        }

        if(vm.count("json"))
        {
            std::string body;
            for(core::string_view data :
                vm.at("json").as<std::vector<std::string>>())
            {
                if(data.starts_with('@'))
                    any_istream{ data.substr(1) }.append_to(body);
                else
                    body.append(data);
            }
            msg = string_body{ std::move(body),
                "application/x-www-form-urlencoded" };
        }

        if(vm.count("upload-file"))
        {
            msg = [&]()-> message
            {
                auto path = vm.at("upload-file").as<std::string>();
                if(path == "-")
                    return stdin_body{};

                // Append the filename to the URL if it
                // doesn't already end with one
                auto segs = url.encoded_segments();
                if(segs.empty())
                {
                    segs.push_back(::filename(path));
                }
                else if(auto back = --segs.end(); back->empty())
                {
                    segs.replace(back, ::filename(path));
                }

                return file_body{ std::move(path) };
            }();
        }

        auto cookie_jar       = std::optional<::cookie_jar>{};
        auto explicit_cookies = std::string{};

        if(vm.count("cookie") || vm.count("cookie-jar"))
            cookie_jar.emplace();

        if(vm.count("cookie"))
        {
            for(core::string_view option : vm.at("cookie").as<std::vector<std::string>>())
            {
                // empty options are allowerd and just activates cookie engine
                if(option.find('=') != core::string_view::npos)
                {
                    if(!explicit_cookies.ends_with(';'))
                        explicit_cookies.push_back(';');
                    explicit_cookies.append(option);
                }
                else if(!option.empty())
                {
                    any_istream{ option } >> cookie_jar.value();
                }
            }
        }

        if(vm.count("junk-session-cookies") && cookie_jar.has_value())
            cookie_jar->clear_session_cookies();

        auto max_redirects = [&]() -> std::uint32_t
        {
            if(!vm.count("max-redirs"))
                return 50; // default

            auto arg = vm.at("max-redirs").as<std::int32_t>();
            if(arg < 0)
                return std::numeric_limits<std::uint32_t>::max();
            return static_cast<std::uint32_t>(arg);
        }();

        bool show_headers =
            vm.count("head") || vm.count("include") || vm.count("show-headers");

        if(show_headers && vm.count("remote-header-name"))
            throw std::runtime_error{
                "showing headers and --remote-header-name cannot be combined" };

        auto timeout = vm.count("max-time")
            ? ch::duration_cast<ch::steady_clock::duration>(
                ch::duration<float>(vm.at("max-time").as<float>()))
            : ch::steady_clock::duration::max();

        auto allowed_redirect_protos = [&]() -> std::set<urls::scheme>
        {
            if(!vm.count("proto-redir"))
                return { urls::scheme::http, urls::scheme::https };

            std::set<urls::scheme> rs;
            for(auto& s : vm.at("proto-redir").as<std::vector<std::string>>())
            {
                if(s == "http")
                    rs.emplace(urls::scheme::http);
                if(s == "https")
                    rs.emplace(urls::scheme::https);
            }
            return rs;
        }();

        asio::co_spawn(
            ioc,
            request(
                vm,
                body_output,
                header_output,
                allowed_redirect_protos,
                max_redirects,
                show_headers,
                msg,
                cookie_jar,
                explicit_cookies,
                ssl_ctx,
                http_proto_ctx,
                create_request(vm, msg, url),
                url),
            asio::cancel_after(
                timeout,
                [&](std::exception_ptr ep)
                {
                    if(ep)
                    {
                        if(vm.count("remove-on-error"))
                            body_output.remove_file();
                        std::rethrow_exception(ep);
                    }
                }));

        ioc.run();

        if(vm.count("cookie-jar"))
        {
            any_ostream{ fs::path{ vm.at("cookie-jar").as<std::string>() } }
                << cookie_jar.value();
        }
    }
    catch(std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
