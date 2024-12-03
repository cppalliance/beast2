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
#include "mime_type.hpp"
#include "multipart_form.hpp"
#include "urlencoded_form.hpp"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/read.hpp>
#include <boost/buffers.hpp>
#include <boost/http_io.hpp>
#include <boost/http_proto.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>

#include <cstdlib>

namespace http_io  = boost::http_io;
using system_error = boost::system::system_error;

#ifdef BOOST_HTTP_PROTO_HAS_ZLIB
inline const bool http_proto_has_zlib = true;
#else
inline const bool http_proto_has_zlib = false;
#endif

core::string_view
target(const urls::url_view& url) noexcept
{
    if(url.encoded_target().empty())
        return "/";

    return url.encoded_target();
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

class json_body
{
    std::string body_;

public:
    void
    append(core::string_view value) noexcept
    {
        body_.append(value);
    }

    void
    append(std::istream& is)
    {
        body_.append(std::istreambuf_iterator<char>{ is }, {});
    }

    core::string_view
    content_type() const noexcept
    {
        return "application/json";
    }

    std::size_t
    content_length() const noexcept
    {
        return body_.size();
    }

    buffers::const_buffer
    body() const noexcept
    {
        return { body_.data(), body_.size() };
    }
};

class message
{
    std::variant<
        std::monostate,
        json_body,
        urlencoded_form,
        multipart_form> body_;
public:
    message() = default;

    message(json_body&& json_body)
        : body_{ std::move(json_body) }
    {
    }

    message(urlencoded_form&& form)
        : body_{ std::move(form) }
    {
    }

    message(multipart_form&& form)
        : body_{ std::move(form) }
    {
    }

    void
    set_headers(http_proto::request& req) const
    {
        std::visit(
        [&](auto& f)
        {
            if constexpr(!std::is_same_v<
                decltype(f), const std::monostate&>)
            {
                req.set_method(http_proto::method::post);
                req.set_content_length(f.content_length());
                req.set(
                    http_proto::field::content_type,
                    f.content_type());
            }
        },
        body_);
    }

    void
    start_serializer(
        http_proto::serializer& ser,
        http_proto::request& req) const
    {
        std::visit(
        [&](auto& f)
        {
            if constexpr(std::is_same_v<
                decltype(f), const multipart_form&>)
            {
                ser.start<
                    multipart_form::source>(req, &f);
            }
            else if constexpr(
                std::is_same_v<decltype(f), const json_body&> ||
                std::is_same_v<decltype(f), const urlencoded_form&>)
            {
                ser.start(req, f.body());
            }
            else
            {
                ser.start(req);
            }
        },
        body_);
    }
};

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

    request.set_target(target(url));
    request.set(field::accept, "*/*");
    request.set(field::host, url.host());

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
        for(auto& header : vm.at("header").as<std::vector<std::string>>())
        {
            if(auto pos = header.find(':'); pos != std::string::npos)
                request.set(header.substr(0, pos), header.substr(pos + 1));
        }
    }

    return request;
}

asio::awaitable<void>
request(
    const po::variables_map& vm,
    any_ostream& body_output,
    std::optional<any_ostream>& header_output,
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
    auto stream     = any_stream{ asio::ip::tcp::socket{ executor } };

    auto connect_to = [&](const urls::url_view& url)
    {
        namespace ch = std::chrono;
        auto timeout = vm.count("connect-timeout")
            ? ch::duration_cast<ch::steady_clock::duration>(
                ch::duration<float>(vm.at("connect-timeout").as<float>()))
            : ch::steady_clock::duration::max();

        return asio::co_spawn(
            executor,
            connect(vm, ssl_ctx, http_proto_ctx, stream, url),
            asio::cancel_after(timeout));
    };

    auto set_cookies = [&](const urls::url_view& url)
    {
        auto field = cookie_jar ? cookie_jar->make_field(url) : std::string{};
        field.append(explicit_cookies);
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
        if(vm.count("head") || vm.count("include"))
            body_output << parser.get().buffer();

        if(header_output.has_value())
            header_output.value() << parser.get().buffer();
    };

    co_await connect_to(url);

    set_cookies(url);
    msg.start_serializer(serializer, request);
    co_await http_io::async_write(stream, serializer);

    parser.reset();
    parser.start();
    co_await http_io::async_read_header(stream, parser);
    extract_cookies(url);
    stream_headers();

    // handle redirects
    auto referer = urls::url{ url };
    for(;;)
    {
        auto [is_redirect, need_method_change] =
            ::is_redirect(vm, parser.get().status());

        if(!is_redirect || !vm.count("location"))
            break;

        auto response = parser.get();
        if(auto it = response.find(field::location);
           it != response.end())
        {
            auto location = urls::url{};

            urls::resolve(
                referer,
                urls::parse_uri_reference(it->value).value(),
                location);

            if(can_reuse_connection(response, referer, location))
            {
                // Discard body
                if(request.method() != http_proto::method::head)
                {
                    while(!parser.is_complete())
                    {
                        parser.consume_body(
                            buffers::buffer_size(parser.pull_body()));
                        co_await http_io::async_read_some(stream, parser);
                    }
                }
            }
            else
            {
                if(!vm.count("proxy"))
                    co_await stream.async_shutdown(asio::as_tuple);

                co_await connect_to(location);
            }

            // Change the method according to RFC 9110, Section 15.4.4.
            if(need_method_change && !vm.count("head"))
            {
                request.set_method(http_proto::method::get);
                request.set_content_length(0);
                request.erase(field::content_type);
                msg = {}; // drop the body
            }
            request.set_target(target(location));
            request.set(field::host, location.host());
            request.set(field::referer, referer);

            // Update the cookies for the new url
            request.erase(field::cookie);
            set_cookies(location);

            referer = location;

            serializer.reset();
            msg.start_serializer(serializer, request);
            co_await http_io::async_write(stream, serializer);

            parser.reset();
            parser.start();
            co_await http_io::async_read_header(stream, parser);
            extract_cookies(location);
            stream_headers();
        }
        else
        {
            throw std::runtime_error{ "Bad redirect response" };
        }
    }

    // stream body
    if(request.method() != http_proto::method::head)
    {
        for(;;)
        {
            for(auto cb : parser.pull_body())
            {
                body_output << core::string_view{
                    static_cast<const char*>(cb.data()), cb.size() };
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
    {
        auto [ec] = co_await stream.async_shutdown(asio::as_tuple);
        if(ec && ec != ssl::error::stream_truncated)
            throw system_error{ ec };
    }
};

int
main(int argc, char* argv[])
{
    try
    {
        auto odesc = po::options_description{"Options"};
        odesc.add_options()
            ("cacert",
                po::value<std::string>()->value_name("<file>"),
                "CA certificate to verify peer against")
            ("capath",
                po::value<std::string>()->value_name("<dir>"),
                "CA directory to verify peer against")
            ("cert,E",
                po::value<std::string>()->value_name("<certificate>"),
                "Client certificate file")
            ("compressed", "Request compressed response")
            ("connect-timeout",
                po::value<float>()->value_name("<frac sec>"),
                "Maximum time allowed for connection")
            ("continue-at,C",
                po::value<std::uint64_t>()->value_name("<offset>"),
                "Resume transfer offset")
            ("cookie,b",
                po::value<std::vector<std::string>>()->value_name("<data|filename>"),
                "Send cookies from string/file")
            ("cookie-jar,c",
                po::value<std::string>()->value_name("<filename>"),
                "Write cookies to <filename> after operation")
            ("data,d",
                po::value<std::vector<std::string>>()->value_name("<data>"),
                "HTTP POST data")
            ("dump-header,D",
                po::value<std::string>()->value_name("<filename>"),
                "Write the received headers to <filename>")
            ("form,F",
                po::value<std::vector<std::string>>()->value_name("<name=content>"),
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
            ("json",
                po::value<std::vector<std::string>>()->value_name("<data>"),
                "HTTP POST JSON")
            ("junk-session-cookies,j", "Ignore session cookies read from file")
            ("location,L", "Follow redirects")
            ("no-keepalive", "Disable TCP keepalive on the connection")
            ("output,o",
                po::value<std::string>()->value_name("<file>"),
                "Write to file instead of stdout")
            ("pass",
                po::value<std::string>()->value_name("<phrase>"),
                "Passphrase for the private key")
            ("post301", "Do not switch to GET after following a 301")
            ("post302", "Do not switch to GET after following a 302")
            ("post303", "Do not switch to GET after following a 303")
            ("proxy,x",
                po::value<std::string>()->value_name("<url>"),
                "Use this proxy")
            ("range,r",
                po::value<std::string>()->value_name("<range>"),
                "Retrieve only the bytes within range")
            ("referer,e",
                po::value<std::string>()->value_name("<url>"),
                "Referer URL")
            ("request,X",
                po::value<std::string>()->value_name("<method>"),
                "Specify request method to use")
            ("tcp-nodelay", "Use the TCP_NODELAY option")
            ("tls-max",
                po::value<std::string>()->value_name("<version>"),
                "Set maximum allowed TLS version")
            ("tlsv1.0", "Use TLSv1.0 or greater")
            ("tlsv1.1", "Use TLSv1.1 or greater")
            ("tlsv1.2", "Use TLSv1.2 or greater")
            ("tlsv1.3", "Use TLSv1.3 or greater")
            ("include,i", "Include protocol response headers in the output")
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
            cfg.body_limit = std::numeric_limits<std::size_t>::max();
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
            if(vm.count("output"))
                return any_ostream{ vm.at("output").as<std::string>() };
            return any_ostream{ "-" };
        }();

        auto header_output = [&]() -> std::optional<any_ostream>
        {
            if(vm.count("dump-header"))
                return any_ostream{ vm.at("dump-header").as<std::string>() };
            return std::nullopt;
        }();

        auto msg = message{};

        if((!!vm.count("form") + !!vm.count("data") + !!vm.count("json")) == 2)
            throw std::runtime_error{
                "You can only select one HTTP request method"};

        if(vm.count("form"))
        {
            auto form = multipart_form{};
            for(auto& data : vm.at("form").as<std::vector<std::string>>())
            {
                if(auto pos = data.find('='); pos != std::string::npos)
                {
                    auto name  = core::string_view{ data }.substr(0, pos);
                    auto value = core::string_view{ data }.substr(pos + 1);
                    if(!value.empty() && value[0] == '@')
                    {
                        form.append_file(
                            name,
                            value.substr(1),
                            std::string{ mime_type(value.substr(1)) });
                    }
                    else
                    {
                        form.append_text(name, value, boost::none);
                    }
                }
                else
                {
                    throw std::runtime_error{
                        "Illegally formatted input field"};
                }
            }
            msg = std::move(form);
        }

        if(vm.count("data"))
        {
            if(vm.count("get"))
            {
                for(auto data : vm.at("data").as<std::vector<std::string>>())
                {
                    if(auto pos = data.find('='); pos != std::string::npos)
                    {
                        url.params().append(
                            { data.substr(0, pos), data.substr(pos + 1) });
                    }
                    else
                    {
                        url.params().append({ data, urls::no_value });
                    }
                }
            }
            else
            {
                auto form = urlencoded_form{};
                for(auto& data : vm.at("data").as<std::vector<std::string>>())
                {
                    if(!data.empty() && data[0] == '@')
                    {
                        form.append(any_istream{ data.substr(1) });
                    }
                    else
                    {
                        if(auto pos = data.find('='); pos != std::string::npos)
                        {
                            form.append(
                                data.substr(0, pos),
                                data.substr(pos + 1));
                        }
                        else
                        {
                            form.append(data, "");
                        }
                    }
                }
                msg = std::move(form);
            }
        }

        if(vm.count("json"))
        {
            auto body = json_body{};
            for(auto& data : vm.at("json").as<std::vector<std::string>>())
            {
                if(data.empty())
                    continue;

                if(data[0] == '@')
                {
                    body.append(any_istream{ data.substr(1) });
                }
                else
                {
                    body.append(data);
                }
            }
            msg = std::move(body);
        }

        auto cookie_jar       = std::optional<::cookie_jar>{};
        auto explicit_cookies = std::string{};

        if(vm.count("cookie") || vm.count("cookie-jar"))
            cookie_jar.emplace();

        if(vm.count("cookie"))
        {
            for(auto& option : vm.at("cookie").as<std::vector<std::string>>())
            {
                // empty options are allowerd and just activates cookie engine
                if(option.find('=') != std::string::npos)
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

        asio::co_spawn(
            ioc,
            request(
                vm,
                body_output,
                header_output,
                msg,
                cookie_jar,
                explicit_cookies,
                ssl_ctx,
                http_proto_ctx,
                create_request(vm, msg, url),
                url),
            [](std::exception_ptr ep)
            {
                if(ep)
                    std::rethrow_exception(ep);
            });

        ioc.run();

        if(vm.count("cookie-jar"))
        {
            any_ostream{ vm.at("cookie-jar").as<std::string>() }
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
