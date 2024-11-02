//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/http_io.hpp>
#include <boost/http_proto.hpp>
#include <boost/program_options.hpp>
#include <boost/url.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

#include <variant>

namespace asio       = boost::asio;
namespace core       = boost::core;
namespace http_io    = boost::http_io;
namespace http_proto = boost::http_proto;
namespace po         = boost::program_options;
namespace ssl        = boost::asio::ssl;
namespace urls       = boost::urls;

#ifdef BOOST_HTTP_PROTO_HAS_ZLIB
inline const bool http_proto_has_zlib = true;
#else
inline const bool http_proto_has_zlib = false;
#endif

class any_stream
{
public:
    using executor_type     = asio::any_io_executor;
    using plain_stream_type = asio::ip::tcp::socket;
    using ssl_stream_type   = ssl::stream<plain_stream_type>;

    explicit any_stream(plain_stream_type stream)
        : stream_{ std::move(stream) }
    {
    }

    explicit any_stream(ssl_stream_type stream)
        : stream_{ std::move(stream) }
    {
    }

    executor_type
    get_executor() noexcept
    {
        return std::visit([](auto& s) { return s.get_executor(); }, stream_);
    }

    template<
        typename ConstBufferSequence,
        typename CompletionToken =
            asio::default_completion_token_t<executor_type>>
    auto
    async_write_some(
        const ConstBufferSequence& buffers,
        CompletionToken&& token =
            asio::default_completion_token_t<executor_type>{})
    {
        return boost::asio::async_compose<
            CompletionToken,
            void(boost::system::error_code, size_t)>(
            [this, buffers, init = false](
                auto&& self,
                boost::system::error_code ec = {},
                size_t n                     = 0) mutable
            {
                if(std::exchange(init, true))
                    return self.complete(ec, n);

                std::visit(
                    [&](auto& s)
                    { s.async_write_some(buffers, std::move(self)); },
                    stream_);
            },
            token,
            get_executor());
    }

    template<
        typename MutableBufferSequence,
        typename CompletionToken =
            asio::default_completion_token_t<executor_type>>
    auto
    async_read_some(
        const MutableBufferSequence& buffers,
        CompletionToken&& token =
            asio::default_completion_token_t<executor_type>{})
    {
        return boost::asio::async_compose<
            CompletionToken,
            void(boost::system::error_code, size_t)>(
            [this, buffers, init = false](
                auto&& self,
                boost::system::error_code ec = {},
                size_t n                     = 0) mutable
            {
                if(std::exchange(init, true))
                    return self.complete(ec, n);

                std::visit(
                    [&](auto& s)
                    { s.async_read_some(buffers, std::move(self)); },
                    stream_);
            },
            token,
            get_executor());
    }

    template<
        typename CompletionToken =
            asio::default_completion_token_t<executor_type>>
    auto
    async_shutdown(
        CompletionToken&& token =
            asio::default_completion_token_t<executor_type>{})
    {
        return boost::asio::
            async_compose<CompletionToken, void(boost::system::error_code)>(
                [this, init = false](
                    auto&& self, boost::system::error_code ec = {}) mutable
                {
                    if(std::exchange(init, true))
                        return self.complete(ec);

                    std::visit(
                        [&](auto& s)
                        {
                            if constexpr(
                                std::is_same_v<decltype(s),ssl_stream_type&>)
                            {
                                s.async_shutdown(std::move(self));
                            }
                            else
                            {
                                s.close(ec);
                                asio::async_immediate(
                                    s.get_executor(),
                                    asio::append(std::move(self), ec));
                            }
                        },
                        stream_);
                },
                token,
                get_executor());
    }

private:
    std::variant<plain_stream_type, ssl_stream_type> stream_;
};

class output_stream
{
    std::ofstream file_;

public:
    output_stream() = default;

    explicit output_stream(core::string_view path)
    {
        file_.exceptions(std::ofstream::badbit);
        file_.open(path, std::ios::binary);
        if(!file_.is_open())
            throw std::runtime_error{ "Couldn't open the output file" };
    }

    void
    write(auto buf)
    {
        if(file_.is_open())
        {
            file_.write(static_cast<const char*>(buf.data()), buf.size());
            return;
        }
        std::cout.write(static_cast<const char*>(buf.data()), buf.size());
    }
};

asio::awaitable<any_stream>
connect(ssl::context& ssl_ctx, urls::url_view url)
{
    auto executor = co_await asio::this_coro::executor;
    auto resolver = asio::ip::tcp::resolver{ executor };
    auto service  = url.has_port() ? url.port() : url.scheme();
    auto rresults = co_await resolver.async_resolve(url.host(), service);

    if(url.scheme() == "https")
    {
        auto stream = ssl::stream<asio::ip::tcp::socket>{ executor, ssl_ctx };
        co_await asio::async_connect(stream.lowest_layer(), rresults);

        if(auto host_s = std::string{ url.host() };
           !SSL_set_tlsext_host_name(stream.native_handle(), host_s.c_str()))
        {
            throw boost::system::system_error(
                static_cast<int>(::ERR_get_error()),
                asio::error::get_ssl_category());
        }

        co_await stream.async_handshake(ssl::stream_base::client);
        co_return stream;
    }

    auto stream = asio::ip::tcp::socket{ executor };
    co_await asio::async_connect(stream, rresults);
    co_return stream;
}

auto
is_redirect(http_proto::status status) noexcept
{
    struct result_t
    {
        bool is_redirect;
        bool need_method_change;
    };

    // The specifications do not intend for 301 and 302 redirects to change the
    // HTTP method, but most user agents do change the method in practice.
    switch(status)
    {
        case http_proto::status::moved_permanently:
        case http_proto::status::found:
        case http_proto::status::see_other:
            return result_t{ true, true };
        case http_proto::status::temporary_redirect:
        case http_proto::status::permanent_redirect:
            return result_t{ true, false };
        default:
            return result_t{ false, false };
    }
}

core::string_view
get_target(urls::url_view url) noexcept
{
    if(url.encoded_target().empty())
        return "/";

    return url.encoded_target();
}

http_proto::request
create_request(const po::variables_map& vm, urls::url_view url)
{
    using http_proto::field;
    using http_proto::method;
    using http_proto::version;

    auto request = http_proto::request{};

    request.set_method(vm.count("head") ? method::head : method::get);

    if(vm.count("request"))
        request.set_method(vm.at("request").as<std::string>());

    request.set_version(
        vm.count("http1.0") ? version::http_1_0 : version::http_1_1);

    request.set_target(get_target(url));
    request.set(field::accept, "*/*");
    request.set(field::host, url.host());

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

    if(vm.count("user"))
    {
        // TODO: use base64 encoding for basic authentication
        request.set(field::authorization, vm.at("user").as<std::string>());
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
    output_stream& output,
    ssl::context& ssl_ctx,
    http_proto::context& http_proto_ctx,
    http_proto::request request,
    urls::url_view url)
{
    auto stream     = co_await connect(ssl_ctx, url);
    auto parser     = http_proto::response_parser{ http_proto_ctx };
    auto serializer = http_proto::serializer{ http_proto_ctx };

    serializer.start(request);
    co_await http_io::async_write(stream, serializer);

    parser.reset();
    parser.start();
    co_await http_io::async_read_header(stream, parser);

    // handle redirects
    auto referer_url = urls::url{ url };
    for(;;)
    {
        auto [is_redirect, need_method_change] =
            ::is_redirect(parser.get().status());

        if(!is_redirect || !vm.count("location"))
            break;

        auto response = parser.get();
        if(auto it = response.find(http_proto::field::location);
           it != response.end())
        {
            auto redirect_url = urls::parse_uri(it->value).value();

            // TODO: reuse the established connection when possible
            co_await stream.async_shutdown(asio::as_tuple);
            stream = co_await connect(ssl_ctx, redirect_url);

            // Change the method according to RFC 9110, Section 15.4.4.
            if(need_method_change && !vm.count("head"))
            {
                request.set_method(http_proto::method::get);
                // TODO: drop the request body
            }
            request.set_target(get_target(redirect_url));
            request.set(http_proto::field::host, redirect_url.host());
            request.set(http_proto::field::referer, referer_url);

            referer_url = redirect_url;

            serializer.reset();
            serializer.start(request);
            co_await http_io::async_write(stream, serializer);

            parser.reset();
            parser.start();
            co_await http_io::async_read_header(stream, parser);
        }
        else
        {
            throw std::runtime_error{ "Bad redirect response" };
        }
    }

    // stream headers
    if(vm.count("head") || vm.count("show-headers"))
        output.write(parser.get().buffer());

    // stream body
    if(request.method() != http_proto::method::head)
    {
        for(;;)
        {
            for(auto cb : parser.pull_body())
            {
                output.write(cb);
                parser.consume_body(cb.size());
            }

            if(parser.is_complete())
                break;

            auto [ec, _] =
                co_await http_io::async_read_some(stream, parser, asio::as_tuple);
            if(ec && ec != http_proto::condition::need_more_input)
                throw boost::system::system_error{ ec };
        }
    }

    // clean shutdown
    auto [ec] = co_await stream.async_shutdown(asio::as_tuple);
    if(ec && ec != ssl::error::stream_truncated)
        throw boost::system::system_error{ ec };
};

int
main(int argc, char* argv[])
{
    try
    {
        auto odesc = po::options_description{"Options"};
        odesc.add_options()
            ("help,h", "produce help message")
            ("head,I", "Show document info only")
            ("header,H",
                po::value<std::vector<std::string>>()->value_name("<header>"),
                "Pass custom header(s) to server")
            ("location,L", "Follow redirects")
            ("continue-at,C",
                po::value<std::uint64_t>()->value_name("<offset>"),
                "Resume transfer offset")
            ("range,r",
                po::value<std::string>()->value_name("<range>"),
                "Retrieve only the bytes within range")
            ("output,o",
                po::value<std::string>()->value_name("<file>"),
                "Write to file instead of stdout")
            ("request,X",
                po::value<std::string>()->value_name("<method>"),
                "Specify request method to use")
            ("show-headers,i", "Show response headers in the output")
            ("referer,e",
                po::value<std::string>()->value_name("<url>"),
                "Referer URL")
            ("user,u",
                po::value<std::string>()->value_name("<user:password>"),
                "Server user and password")
            ("user-agent,A",
                po::value<std::string>()->value_name("<name>"),
                "Send User-Agent <name> to server")
            ("url",
                po::value<std::string>()->value_name("<url>"),
                "URL to work with")
            ("compressed", "Request compressed response")
            ("http1.0", "Use HTTP 1.0");

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
                << "Usage: flex_await [options...] <url>\n"
                << "Example:\n"
                << "    flex_await https://www.example.com\n"
                << "    flex_await -L http://httpstat.us/301\n"
                << odesc;
            return EXIT_FAILURE;
        }

        auto url = urls::parse_uri(vm.at("url").as<std::string>());
        if(url.has_error())
        {
            std::cerr
                << "Failed to parse URL\n"
                << "Error: " << url.error().what() << std::endl;
            return EXIT_FAILURE;
        }

        auto ioc            = asio::io_context{};
        auto ssl_ctx        = ssl::context{ ssl::context::tlsv12_client };
        auto http_proto_ctx = http_proto::context{};

        ssl_ctx.set_verify_mode(ssl::verify_none);

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

        auto output = [&]
        {
            if(vm.count("output"))
                return output_stream{ vm.at("output").as<std::string>() };
            return output_stream{};
        }();

        asio::co_spawn(
            ioc,
            request(
                vm,
                output,
                ssl_ctx,
                http_proto_ctx,
                create_request(vm, url.value()),
                url.value()),
            [](std::exception_ptr ep)
            {
                if(ep)
                    std::rethrow_exception(ep);
            });

        ioc.run();
    }
    catch(std::exception const& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

#else

int
main(int, char*[])
{
    std::cerr << "Coroutine examples require C++20" << std::endl;
    return EXIT_FAILURE;
}

#endif
