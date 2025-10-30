//
// Copyright (c) 2025 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/host_name_verification.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/core/ignore_unused.hpp>
#include <boost/beast2.hpp>
#include <boost/http_proto/request.hpp>
#include <boost/http_proto/response_parser.hpp>
#include <boost/http_proto/serializer.hpp>
#include <boost/rts/brotli/decode.hpp>
#include <boost/rts/context.hpp>
#include <boost/rts/zlib/inflate.hpp>
#include <boost/url/parse.hpp>
#include <boost/url/url.hpp>

#include <iostream>
#include <functional>

using namespace boost;
using namespace std::placeholders;

// Sends a GET request to the specified URL and
// writes the response to standard output.
// Automatically follows HTTP redirects.
class session
{
    asio::ssl::context& ssl_ctx_;
    asio::ssl::stream<asio::ip::tcp::socket> stream_;
    asio::ip::tcp::resolver resolver_;
    http_proto::serializer sr_;
    http_proto::response_parser pr_;
    http_proto::request req_;
    urls::url url_;
    std::string host_;
    std::string port_;
    std::uint32_t max_redirects_ = 50;
    bool secure_ = false;

public:
    session(
        asio::io_context& ioc,
        asio::ssl::context& ssl_ctx,
        const rts::context& rts_ctx)
        : ssl_ctx_(ssl_ctx)
        , stream_(ioc, ssl_ctx)
        , resolver_(ioc)
        , sr_(rts_ctx)
        , pr_(rts_ctx)
    {
    }

    void
    run(urls::url_view url)
    {
        using field = http_proto::field;

        // Set up an HTTP GET request
        if(!url.encoded_target().empty())
            req_.set_target(url.encoded_target());

        req_.set(field::user_agent, "Boost.Http.Io");
        req_.set(
            field::host,
            url.authority().encoded_host_and_port().decode());

        // Enable compression
    #ifdef BOOST_RTS_HAS_BROTLI
        req_.append(field::accept_encoding, "br");
    #endif
    #ifdef BOOST_RTS_HAS_ZLIB
        req_.append(field::accept_encoding, "deflate, gzip");
    #endif

        // Keep original URL for possible redirect
        url_ = url;

        do_resolve();
    }

private:
    void
    do_resolve()
    {
        // Reconstruct stream in case this is a redirect 
        stream_ = { stream_.get_executor(), ssl_ctx_ };

        // Use SSL stream if URL scheme is HTTPS
        secure_ = (url_.scheme_id() == urls::scheme::https);

        // Set the port for resolve operation
        if(url_.has_port())
            port_ = url_.port();
        else if(url_.scheme_id() == urls::scheme::https)
            port_ = "443";
        else if(url_.scheme_id() == urls::scheme::http) 
            port_ = "80";
        else
            return fail("Unsupported URL scheme");

        // Set the host for resolve operation
        host_ = url_.host();

        // Look up the domain name
        resolver_.async_resolve(
            host_,
            port_,
            std::bind(&session::on_resolve, this, _1, _2));
    }

    void
    on_resolve(
        system::error_code ec,
        asio::ip::tcp::resolver::results_type results)
    {
        if(ec)
            return fail(ec, "resolve");

        // Make the connection on the IP address we get from a lookup
        asio::async_connect(
            stream_.next_layer(),
            results,
            std::bind(&session::on_connect, this, _1, _2));
    }

    void
    on_connect(
        system::error_code ec,
        asio::ip::tcp::resolver::results_type::endpoint_type)
    {
        if(ec)
            return fail(ec, "connect");

        if(secure_)
        {
            // Set SNI Hostname (many hosts need this to handshake successfully)
            if(!::SSL_set_tlsext_host_name(stream_.native_handle(), host_.c_str()))
            {
                return fail(
                    { static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category() },
                    "SSL_set_tlsext_host_name");
            }

            // Set the expected hostname in the peer certificate for verification
            stream_.set_verify_callback(asio::ssl::host_name_verification(host_));

            // Perform the SSL handshake
            stream_.async_handshake(
                asio::ssl::stream_base::client,
                std::bind(&session::on_ssl_handshake, this, _1));
        }
        else
        {
            do_write();
        }
    }

    void
    on_ssl_handshake(system::error_code ec)
    {
        if(ec)
            return fail(ec, "handshake");

        do_write();
    }

    void
    do_write()
    {
        // Prepare serializer
        sr_.start(req_);

        // Send the HTTP request to the remote host
        if(secure_)
            beast2::async_write(
                stream_,
                sr_,
                std::bind(&session::on_write, this, _1, _2));
        else
            beast2::async_write(
                stream_.next_layer(),
                sr_,
                std::bind(&session::on_write, this, _1, _2));
    }

    void
    on_write(
        system::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec)
            return fail(ec, "write");

        // Start parser for a new message
        pr_.reset();
        pr_.start();

        // Receive the HTTP response header
        if(secure_)
            beast2::async_read_header(
                stream_,
                pr_,
                std::bind(&session::on_read_header, this, _1, _2));
        else
            beast2::async_read_header(
                stream_.next_layer(),
                pr_,
                std::bind(&session::on_read_header, this, _1, _2));
    }

    void
    on_read_header(
        system::error_code ec,
        std::size_t bytes_transferred)
    {
        boost::ignore_unused(bytes_transferred);

        if(ec)
            return fail(ec, "read_header");

        // Handle redirects
        if(is_redirect(pr_.get().status()))
            return on_redirect_response(pr_.get());

        // Write the message to standard out
        pr_.set_body<stdout_sink>();

        // Receive the HTTP response body
        if(secure_)
            beast2::async_read(
                stream_,
                pr_,
                std::bind(&session::on_read, this, _1, _2));
        else
            beast2::async_read(
                stream_.next_layer(),
                pr_,
                std::bind(&session::on_read, this, _1, _2));
    }

    void
    on_redirect_response(
        http_proto::response_base const& response)
    {
        using field = http_proto::field;

        if(max_redirects_ == 0)
            return fail("Maximum redirects followed");

        --max_redirects_;

        auto it = response.find(field::location);
        if(it == response.end())
            return fail("Bad redirect response");

        auto rs = urls::parse_uri_reference(it->value);
        if(rs.has_error())
            return fail(rs.error(), "Bad redirect URL");

        // URL May be relative to the request URL or an absolute URL.
        urls::url redirect;
        urls::resolve(url_, rs.value(), redirect);

        // Prepare the request to follow the redirect
        req_.set_target(redirect.encoded_target());
        req_.set(field::referer, url_);
        req_.set(
            field::host,
            redirect.authority().encoded_host_and_port().decode());

        // Update URL
        bool reuse_conn = can_reuse_connection(response, url_, redirect);
        url_ = redirect;

        if(reuse_conn)
        {
            // Read and discard bodies if they are <= 32KB
            // Open a new connection otherwise.
            pr_.set_body_limit(32 * 1024);
            return beast2::async_read(
                stream_,
                pr_,
                [this](system::error_code ec, std::size_t)
                {
                    // Open a new connection on failure
                    if(ec)
                        return do_resolve();

                    // Perform the request on the same stream
                    do_write();
                });
        }

        // Gracefully close the stream before reopening
        if(secure_)
        {
            stream_.async_shutdown(
                [this](system::error_code)
                {
                    do_resolve();
                });
        }
        else
        {
            system::error_code ec;
            stream_.next_layer().shutdown(
                asio::ip::tcp::socket::shutdown_both,
                ec);
            do_resolve();
        }
    }

    void
    on_read(
        system::error_code ec,
        std::size_t bytes_transferred)
    {
        if(ec)
            return fail(ec, "read");

        // Gracefully close the stream
        if(secure_)
        {
            stream_.async_shutdown(
                [](system::error_code ec)
                {
                    if(ec.failed() && ec != asio::ssl::error::stream_truncated)
                        return fail(ec, "shutdown");
                });
        }
        else
        {
            stream_.next_layer().shutdown(
                asio::ip::tcp::socket::shutdown_both,
                ec);

            // not_connected happens sometimes so don't bother reporting it.
            if(ec && ec != system::errc::not_connected)
                return fail(ec, "shutdown");
        }
    }

    static
    void
    fail(system::error_code ec, const char* operation)
    {
        std::cerr << operation << ": " << ec.message() << "\n";
    }

    static
    void
    fail(const char* what)
    {
        std::cerr << what << "\n";;
    }

    static
    bool
    is_redirect(http_proto::status s) noexcept
    {
        using status = http_proto::status;
        switch(s)
        {
        case status::moved_permanently:
        case status::found:
        case status::see_other:
        case status::temporary_redirect:
        case status::permanent_redirect:
            return true;
        default:
            return false;
        }
    }

    // A connection can be reused only if:
    // 1. Both URLs share the same origin.
    // 2. The response uses HTTP/1.1.
    // 3. The response does not include a `Connection: close` header.
    static
    bool
    can_reuse_connection(
        http_proto::response_base const& response,
        urls::url_view a,
        urls::url_view b) noexcept
    {
        if(a.encoded_origin() != b.encoded_origin())
            return false;

        if(response.version() != http_proto::version::http_1_1)
            return false;

        if(response.metadata().connection.close)
            return false;

        return true;
    }

    // Writes body to standard out
    struct stdout_sink : http_proto::sink
    {
        results
        on_write(buffers::const_buffer cb, bool) override
        {
            std::cout.write(
                static_cast<const char*>(cb.data()), cb.size());
            return { {}, cb.size() };
        }
    };
};

int
main(int argc, char* argv[])
{
    // Check command line arguments.
    if(argc != 2)
    {
    help:
        std::cerr <<
            "Usage: async_get <url>\n" <<
            "Example:\n" <<
            "    async_get https://example.com\n"
            "    async_get http://httpforever.com\n";
        return EXIT_FAILURE;
    }

    // The io_context is required for all I/O
    asio::io_context ioc;

    // The SSL context is required, and holds certificates
    asio::ssl::context ssl_ctx(asio::ssl::context::tls_client);

    // RTS context holds optional deflate and
    // required configuration services
    rts::context rts_ctx;

    // Install parser service
    {
        http_proto::response_parser::config cfg;
        cfg.body_limit = std::uint64_t(-1);
        cfg.min_buffer = 64 * 1024;
    #ifdef BOOST_RTS_HAS_BROTLI
        cfg.apply_brotli_decoder  = true;
        rts::brotli::install_decode_service(rts_ctx);
    #endif
    #ifdef BOOST_RTS_HAS_ZLIB
        cfg.apply_deflate_decoder = true;
        cfg.apply_gzip_decoder    = true;
        rts::zlib::install_inflate_service(rts_ctx);
    #endif
        http_proto::install_parser_service(rts_ctx, cfg);
    }

    // Install serializer service with default configuration
    http_proto::install_serializer_service(rts_ctx, {});

    // Root certificates used for verification
    ssl_ctx.set_default_verify_paths();

    // Verify the remote server's certificate
    ssl_ctx.set_verify_mode(asio::ssl::verify_peer);

    // Parse the input URL
    urls::url url{ argv[1] };

    if(!url.has_authority())
        goto help;

    session s(ioc, ssl_ctx, rts_ctx);
    s.run(url);

    ioc.run();

    return EXIT_SUCCESS;
}
