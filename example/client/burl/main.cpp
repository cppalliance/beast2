//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include "any_iostream.hpp"
#include "any_stream.hpp"
#include "base64.hpp"
#include "connect.hpp"
#include "cookie.hpp"
#include "error.hpp"
#include "message.hpp"
#include "progress_meter.hpp"
#include "request.hpp"
#include "task_group.hpp"
#include "utils.hpp"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/bind_executor.hpp>
#include <boost/asio/cancel_after.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/buffers.hpp>
#include <boost/beast2.hpp>
#include <boost/http.hpp>
#include <boost/capy/brotli/decode.hpp>
#include <boost/capy/zlib/inflate.hpp>
#include <boost/scope/scope_exit.hpp>
#include <boost/scope/scope_fail.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>

#include <cstdlib>

namespace beast2   = boost::beast2;
namespace scope    = boost::scope;
using system_error = boost::system::system_error;

#ifdef BOOST_CAPY_HAS_ZLIB
constexpr bool capy_has_zlib = true;
#else
constexpr bool capy_has_zlib = false;
#endif

#ifdef BOOST_CAPY_HAS_BROTLI
constexpr bool capy_has_brotli = true;
#else
constexpr bool capy_has_brotli = false;
#endif

void
set_target(
    const operation_config& oc,
    http::request& request,
    const urls::url_view& url)
{
    if(oc.request_target)
    {
        request.set_target(oc.request_target.value());
        return;
    }

    request.set_target(url.encoded_target());
}

struct is_redirect_result
{
    bool is_redirect        = false;
    bool need_method_change = false;
};

is_redirect_result
is_redirect(const operation_config& oc, http::status status) noexcept
{
    // The specifications do not intend for 301 and 302
    // redirects to change the HTTP method, but most
    // user agents do change the method in practice.
    switch(status)
    {
    case http::status::moved_permanently:
        return { true, !oc.post301 };
    case http::status::found:
        return { true, !oc.post302 };
    case http::status::see_other:
        return { true, !oc.post303 };
    case http::status::temporary_redirect:
    case http::status::permanent_redirect:
        return { true, false };
    default:
        return { false, false };
    }
}

bool
is_transient_error(http::status status) noexcept
{
    switch(status)
    {
    case http::status::request_timeout:
    case http::status::too_many_requests:
    case http::status::internal_server_error:
    case http::status::bad_gateway:
    case http::status::service_unavailable:
    case http::status::gateway_timeout:
        return true;
    default:
        return false;
    }
}

bool
can_reuse_connection(
    http::response_base const& response,
    const urls::url_view& a,
    const urls::url_view& b) noexcept
{
    if(a.encoded_origin() != b.encoded_origin())
        return false;

    if(response.version() != http::version::http_1_1)
        return false;

    if(response.metadata().connection.close)
        return false;

    return true;
}

bool
should_ignore_body(
    const operation_config& oc,
    http::response_base const& response) noexcept
{
    if(oc.resume_from && !response.count(http::field::content_range))
        return true;

    return false;
}

boost::optional<std::uint64_t>
body_size(http::response_base const& response)
{
    if(response.payload() == http::payload::size)
        return response.payload_size();
    return boost::none;
}

urls::url
redirect_url(
    http::response_base const& response,
    const urls::url_view& referer)
{
    auto it = response.find(http::field::location);
    if(it != response.end())
    {
        auto rs = urls::parse_uri_reference(it->value);
        if(rs.has_value())
        {
            urls::url url;
            urls::resolve(referer, rs.value(), url);
            return url;
        }
    }
    throw std::runtime_error{ "Bad redirect response" };
}

asio::awaitable<void>
report_progress(progress_meter& pm)
{
    auto executor = co_await asio::this_coro::executor;
    auto timer    = asio::steady_timer{ executor };
    auto report   = [&]()
    {
        std::cerr << "\r[";
        auto pct = pm.pct();
        for(int i = 0; i != 25; i++)
            std::cerr << (i * 4 < pct.value_or(0) ? '#' : '-');
        std::cerr << "] ";

        if(pct)
            std::cerr << pct.value();
        else
            std::cerr << '?';

        std::cerr << "% | " << std::setw(7) << format_size(pm.transfered())
                  << " of ";

        if(auto total = pm.total())
            std::cerr << format_size(total.value());
        else
            std::cerr << '?';

        std::cerr << " | " << std::setw(7) << format_size(pm.cur_rate())
                  << "/s | ";

        auto eta = pm.eta();
        if(eta && eta->hours() <= ch::hours{ 99 })
            std::cerr << eta.value();
        else
            std::cerr << "--:--:--";

        std::cerr << std::flush;
    };

    auto scope_exit = scope::make_scope_exit(
        [&]()
        {
            report();
            std::cerr << '\n';
        });

    for(;;)
    {
        report();
        timer.expires_after(ch::milliseconds{ 250 });
        co_await timer.async_wait();
    }
}

http::request
create_request(
    const operation_config& oc,
    const message& msg,
    const urls::url_view& url)
{
    using field   = http::field;
    using method  = http::method;
    using version = http::version;

    if(oc.disallow_username_in_url && url.has_userinfo())
        throw std::runtime_error(
            "Credentials was passed in the URL when prohibited");

    auto request = http::request{};

    request.set_method(oc.no_body ? method::head : method::get);

    if(oc.customrequest)
        request.set_method(oc.customrequest.value());

    request.set_version(oc.http10 ? version::http_1_0 : version::http_1_1);
    set_target(oc, request, url);

    request.set(field::host, url.authority().encoded_host_and_port().decode());
    request.set(field::user_agent, oc.useragent.value_or("Boost.Http.Io"));
    request.set(field::accept, "*/*");

    msg.set_headers(request);

    if(oc.resume_from)
    {
        request.set(
            field::range,
            "bytes=" + std::to_string(oc.resume_from.value()) + "-");
    }

    if(oc.range)
        request.set(field::range, "bytes=" + oc.range.value());

    if(!oc.referer.empty())
        request.set(field::referer, oc.referer);

    if(auto creds = oc.userpwd.value_or(url.userinfo()); !creds.empty())
    {
        auto basic_auth = std::string{ "Basic " };
        base64_encode(basic_auth, creds);
        request.set(field::authorization, basic_auth);
    }

    if(oc.encoding)
    {
        std::string value;

        const auto append = [&](const char* encoding)
        {
            if(!value.empty())
                value.append(", ");
            value.append(encoding);
        };

        if constexpr(capy_has_brotli)
            append("br");

        if constexpr(capy_has_zlib)
        {
            append("deflate");
            append("gzip");
        }

        if(!value.empty())
            request.set(field::accept_encoding, value);
    }

    for(const auto& [_, name, value] : oc.headers)
        request.set(name, value);

    for(const auto& name : oc.omitheaders)
        request.erase(name);

    return request;
}

class sink : public http::sink
{
    progress_meter* pm_;
    any_ostream* os_;
    bool terminal_binary_ok_;

public:
    sink(progress_meter* pm, any_ostream* os, bool terminal_binary_ok)
        : pm_{ pm }
        , os_{ os }
        , terminal_binary_ok_{ terminal_binary_ok }
    {
    }

    results
    on_write(buffers::const_buffer cb, bool) override
    {
        auto chunk =
            core::string_view(static_cast<const char*>(cb.data()), cb.size());

        if(!terminal_binary_ok_ && os_->is_tty() && chunk.contains('\0'))
            return { error::binary_output_to_tty };

        *os_ << chunk;
        pm_->update(cb.size());
        return { {}, cb.size() };
    }
};

asio::awaitable<http::status>
perform_request(
    operation_config oc,
    boost::optional<any_ostream>& header_output,
    boost::optional<cookie_jar>& cookie_jar,
    core::string_view exp_cookies,
    ssl::context& ssl_ctx,
    capy::polystore& capy_ctx,
    message msg,
    request_opt request_opt)
{
    using field     = http::field;
    auto executor   = co_await asio::this_coro::executor;
    auto stream     = any_stream{ asio::ip::tcp::socket{ executor } };
    auto parser     = http::response_parser{ capy_ctx };
    auto serializer = http::serializer{ capy_ctx };

    urls::url url = [&]()
    {
        auto parse_rs = normalize_and_parse_url(request_opt.url);

        if(parse_rs.has_error())
            throw system_error{ parse_rs.error(), "Failed to parse URL" };

        if(parse_rs->host().empty())
            throw std::runtime_error{ "No host part in the URL" };

        auto params = urls::params_view{ oc.query };
        parse_rs->encoded_params().append(params.begin(), params.end());

        if(parse_rs->path().empty())
            parse_rs->set_path("/");

        return std::move(parse_rs.value());
    }();

    if(!request_opt.input.empty())
    {
        msg = [&]() -> message
        {
            if(request_opt.input == "-")
                return stdin_body{};

            auto path = request_opt.input;

            // Append filename to URL if missing
            auto segs = url.encoded_segments();
            if(segs.empty())
            {
                segs.push_back(path.filename().string());
            }
            else if(auto back = --segs.end(); back->empty())
            {
                segs.replace(back, path.filename().string());
            }

            return file_body{ path.string() };
        }();
    }

    fs::path output_path = [&]()
    {
        fs::path path = oc.output_dir;

        if(request_opt.remotename)
        {
            auto segs = url.encoded_segments();
            if(segs.empty() || segs.back().empty())
                path.append("burl_response");
            else
                path.append(segs.back().begin(), segs.back().end());
        }
        else if(request_opt.output == "-")
        {
            oc.terminal_binary_ok = true;
            path                  = "-";
        }
        else if(request_opt.output.empty())
        {
            path = "-";
        }
        else
        {
            path /= request_opt.output;
        }

        if(path != "-")
        {
            if(oc.resume_from_current)
            {
                oc.resume_from = fs::exists(output_path)
                    ? fs::file_size(output_path)
                    : std::uint64_t{ 0 };
            }

            if(oc.create_dirs)
                fs::create_directories(output_path.parent_path());
        }

        return path;
    }();

    if(oc.skip_existing && fs::exists(output_path))
        co_return http::status::ok;

    auto output     = any_ostream{ output_path, !!oc.resume_from };
    auto request    = create_request(oc, msg, url);
    auto scope_fail = scope::make_scope_fail(
        [&]
        {
            if(oc.rm_partial && output_path != "-")
            {
                output.close();
                fs::remove(output_path);
            }
        });

    auto connect_to = [&](any_stream& stream,
                          const urls::url_view& url) -> asio::awaitable<void>
    {
        // clean shutdown
        if(oc.proxy.empty())
            co_await stream.async_shutdown(
                asio::cancel_after(ch::milliseconds{ 500 }, asio::as_tuple));

        co_await asio::co_spawn(
            executor,
            connect(oc, ssl_ctx, capy_ctx, stream, url),
            asio::cancel_after(oc.connect_timeout));

        if(oc.recvpersecond)
            stream.read_limit(oc.recvpersecond.value());

        if(oc.sendpersecond)
            stream.write_limit(oc.sendpersecond.value());
    };

    auto stream_headers = [&](http::response_base const& response)
    {
        if(oc.show_headers)
            output << response.buffer();

        if(header_output.has_value())
            header_output.value() << response.buffer();
    };

    auto set_cookies = [&](const urls::url_view& url, bool trusted)
    {
        auto cookie = cookie_jar ? cookie_jar->make_field(url) : std::string{};

        if(trusted)
            cookie.append(exp_cookies);

        request.erase(field::cookie);

        if(!cookie.empty())
            request.set(field::cookie, cookie);
    };

    auto extract_cookies = [&](const urls::url_view& url)
    {
        if(!cookie_jar)
            return;

        auto subrange = parser.get().find_all(field::set_cookie);
        for(auto sv : subrange)
            cookie_jar->add(url, parse_cookie(sv).value());
    };

    co_await connect_to(stream, url);
    parser.reset();

    auto org_url   = url;
    auto referer   = url;
    auto trusted   = true;
    auto maxredirs = oc.maxredirs;
    for(;;)
    {
        set_cookies(url, trusted);
        msg.start_serializer(serializer, request);

        if(request.method() == http::method::head)
            parser.start_head_response();
        else
            parser.start();

        co_await async_request(stream, serializer, parser, oc.expect100timeout);

        extract_cookies(url);
        stream_headers(parser.get());

        auto [is_redirect, need_method_change] =
            ::is_redirect(oc, parser.get().status());

        if(!is_redirect || !oc.followlocation)
            break;

        if(maxredirs-- == 0)
            throw std::runtime_error{ "Maximum redirects followed" };

        // Prepare the next request to follow the redirect

        url = redirect_url(parser.get(), referer);

        if(!oc.proto_redir.contains(url.scheme_id()))
            throw std::runtime_error{ "Protocol not supported or disabled" };

        if(can_reuse_connection(parser.get(), referer, url))
        {
            // Read and discard bodies if they are <= 32KB
            // Open a new connection otherwise.
            parser.set_body_limit(32 * 1024);
            auto [ec, _] =
                co_await beast2::async_read(stream, parser, asio::as_tuple);
            if(ec)
                goto reconnect;
        }
        else
        {
        reconnect:
            co_await connect_to(stream, url);
            parser.reset();
        }

        // Change the method according to RFC 9110, Section 15.4.4.
        if(need_method_change && request.method() != http::method::head)
        {
            request.set_method(http::method::get);
            request.erase(field::content_length);
            request.erase(field::content_encoding);
            request.erase(field::content_type);
            request.erase(field::expect);
            msg = {}; // drop the body
        }

        set_target(oc, request, url);

        trusted = (org_url.encoded_origin() == url.encoded_origin()) ||
            oc.unrestricted_auth;

        if(!trusted)
            request.erase(field::authorization);

        if(oc.autoreferer)
        {
            referer.remove_userinfo();
            request.set(field::referer, referer);
        }

        request.set(
            field::host, url.authority().encoded_host_and_port().decode());

        referer = url;
    }

    if(oc.failonerror && parser.get().status_int() >= 400)
        throw std::runtime_error(
            "The requested URL returned error: " +
            std::to_string(parser.get().status_int()));

    // use the server-specified Content-Disposition filename
    if(oc.content_disposition && request_opt.remotename)
    {
        auto subrange = parser.get().find_all(field::content_disposition);
        for(auto sv : subrange)
        {
            auto filepath = extract_filename_form_content_disposition(sv);
            if(filepath.has_value())
            {
                // stripp off the potential path
                auto filename = fs::path{ filepath.value() }.filename();
                if(filename.empty())
                    continue;

                output = any_ostream{ oc.output_dir / filename };
                break;
            }
        }
    }

    if(oc.resume_from &&
       parser.get().status() != http::status::range_not_satisfiable &&
       parser.get().count(field::content_range) == 0)
    {
        throw std::runtime_error(
            "HTTP server doesn't seem to support byte ranges. Cannot resume.");
    }

    if(!should_ignore_body(oc, parser.get()))
    {
        auto pm = progress_meter{ body_size(parser.get()) };
        parser.set_body<sink>(&pm, &output, oc.terminal_binary_ok);

        if(output.is_tty() || oc.parallel_max > 1 || oc.noprogress)
        {
            co_await beast2::async_read(stream, parser);
        }
        else
        {
            auto [order, ec, n, ep] =
                co_await asio::experimental::make_parallel_group(
                    beast2::async_read(stream, parser),
                    co_spawn(executor, report_progress(pm)))
                    .async_wait(
                        asio::experimental::wait_for_one{}, asio::deferred);
            if(ec)
                throw system_error{ ec };
        }
    }

    // clean shutdown
    if(oc.proxy.empty())
        co_await stream.async_shutdown(
            asio::cancel_after(ch::milliseconds{ 500 }, asio::as_tuple));

    if(oc.failwithbody && parser.get().status_int() >= 400)
        throw std::runtime_error(
            "The requested URL returned error: " +
            std::to_string(parser.get().status_int()));

    co_return parser.get().status();
};

asio::awaitable<void>
retry(
    const operation_config& oc,
    std::function<asio::awaitable<http::status>()> request_task)
{
    auto executor = co_await asio::this_coro::executor;
    auto timer    = asio::steady_timer{ executor };
    auto retries  = oc.req_retry;
    auto max_time = oc.retry_maxtime
        ? ch::steady_clock::now() + oc.retry_maxtime.value()
        : ch::steady_clock::time_point::max();

    auto next_delay = [&, backoff = ch::seconds{ 1 }]() mutable
    {
        if(oc.retry_delay)
            return ch::duration_cast<ch::seconds>(oc.retry_delay.value());
        if(backoff < ch::seconds{ 10 * 60 })
            backoff *= 2;
        return backoff;
    };

    auto can_retry = [&](error_code ec)
    {
        if(retries == 0 || max_time < ch::steady_clock::now())
            return false;

        if(oc.retry_all_errors)
            return true;

        if(!ec)
            return true;

        if(ec == asio::error::operation_aborted)
            return true;

        if(oc.retry_connrefused && ec == asio::error::connection_refused)
            return true;

        return false;
    };

    for(;;)
    {
        try
        {
            auto status = co_await asio::co_spawn(
                executor, request_task, asio::cancel_after(oc.timeout));

            if(!is_transient_error(status) || !can_retry({}))
                co_return;

            std::cerr << "HTTP error" << std::endl;
        }
        catch(const system_error& e)
        {
            if(e.code() == error::binary_output_to_tty)
            {
                // clang-format off
                std::cerr << 
                    "Binary output can mess up your terminal.\n"
                    "Use \"--output -\" to tell burl to output it to your terminal anyway, or\n"
                    "consider \"--output <FILE>\" to save to a file." << std::endl;
                // clang-format on
                co_return;
            }

            std::cerr << e.what() << std::endl;
            if(!can_retry(e.code()))
                throw;
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << std::endl;
            throw;
        }

        if(!!(co_await asio::this_coro::cancellation_state).cancelled())
            co_return;

        auto delay = next_delay();
        std::cerr << "Will retry in " << delay << " seconds. " << retries
                  << " retries left." << std::endl;
        retries--;
        timer.expires_after(delay);
        co_await timer.async_wait();
    }
}

asio::awaitable<void>
co_main(int argc, char* argv[])
{
    auto args_rs       = parse_args(argc, argv);
    auto& oc           = args_rs.oc;
    auto& ropt_gen     = args_rs.ropt_gen;
    auto& ssl_ctx      = args_rs.ssl_ctx;

    auto executor      = co_await asio::this_coro::executor;
    auto task_group    = ::task_group{ executor, oc.parallel_max };
    auto capy_ctx       = capy::polystore{};
    auto cookie_jar    = boost::optional<::cookie_jar>{};
    auto header_output = boost::optional<any_ostream>{};
    auto exp_cookies   = std::string{};

    if(oc.nobuffer)
    {
        std::cout.setf(std::ios::unitbuf);
        std::cerr.setf(std::ios::unitbuf);
    }

    // parser service
    {
        http::response_parser::config cfg;
        cfg.body_limit = oc.max_filesize;
        cfg.min_buffer = 1024 * 1024;
        if constexpr(capy_has_brotli)
        {
            cfg.apply_brotli_decoder  = true;
            capy::brotli::install_decode_service(capy_ctx);
        }
        if constexpr(capy_has_zlib)
        {
            cfg.apply_deflate_decoder = true;
            cfg.apply_gzip_decoder    = true;
            capy::zlib::install_inflate_service(capy_ctx);
        }
        http::install_parser_service(capy_ctx, cfg);
    }

    // serializer service
    {
        http::serializer::config cfg;
        cfg.payload_buffer = 1024 * 1024;
        http::install_serializer_service(capy_ctx, cfg);
    }

    if(!oc.headerfile.empty())
        header_output.emplace(oc.headerfile);

    if(oc.enable_cookies)
        cookie_jar.emplace();

    for(auto& s : oc.cookies)
    {
        if(!exp_cookies.empty() && !exp_cookies.ends_with(';'))
            exp_cookies.push_back(';');
        exp_cookies.append(s);
    }

    for(auto& path : oc.cookiefiles)
    {
        if(fs::exists(path))
            any_istream{ path } >> cookie_jar.value();
    }

    if(cookie_jar && oc.cookiesession)
        cookie_jar->clear_session_cookies();

    std::exception_ptr ep;
    try
    {
        while(auto ropt = ropt_gen())
        {
            auto request_task = [&, ropt = std::move(ropt)]()
            {
                return perform_request(
                    oc,
                    header_output,
                    cookie_jar,
                    exp_cookies,
                    ssl_ctx,
                    capy_ctx,
                    oc.msg,
                    ropt.value());
            };

            co_spawn(
                executor,
                retry(oc, std::move(request_task)),
                co_await task_group.async_adapt(
                    [&](std::exception_ptr ep)
                    {
                        if(ep && oc.failearly)
                        {
                            task_group.close();
                            task_group.emit(asio::cancellation_type::terminal);
                        }
                    }));
        }
    }
    catch(const system_error& e)
    {
        if(e.code().category() != task_group_category())
            ep = std::current_exception();
    }
    catch(...)
    {
        ep = std::current_exception();
    }

    if(auto cs = co_await asio::this_coro::cancellation_state; !!cs.cancelled())
    {
        task_group.emit(cs.cancelled());
        cs.clear();
    }

    co_await task_group.async_join();

    if(cookie_jar && !oc.cookiejar.empty())
        any_ostream{ oc.cookiejar } << cookie_jar.value();

    if(ep)
        std::rethrow_exception(ep);
}

int
main(int argc, char* argv[])
{
    try
    {
        auto ioc  = asio::io_context{};
        auto sigs = asio::signal_set{ ioc, SIGINT, SIGTERM };

        asio::experimental::make_parallel_group(
            sigs.async_wait(), asio::co_spawn(ioc, co_main(argc, argv)))
            .async_wait(
                asio::experimental::wait_for_one{},
                asio::bind_executor(
                    ioc,
                    [](auto, auto, auto, std::exception_ptr ep)
                    {
                        if(ep)
                            std::rethrow_exception(ep);
                    }));

        ioc.run();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
