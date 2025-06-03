//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include <cstdlib>
#include <boost/asio/basic_stream_socket.hpp>
#include <boost/asio/compose.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/http_proto/request.hpp>
#include <boost/http_proto/response_parser.hpp>
#include <boost/rts/context.hpp>
#include <boost/url/url_view.hpp>

//------------------------------------------------

namespace boost {

/** Connect to a resource indicated by a URL
*/
template<
    class Executor,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(
        system::error_code,
        asio::ip::tcp::endpoint)) ConnectHandler =
        asio::default_completion_token_t<Executor>
>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(ConnectHandler, void(
    system::error_code,
    asio::ip::tcp::endpoint))
async_connect(
    asio::basic_socket<asio::ip::tcp, Executor>& s,
    urls::url_view const& url,
    ConnectHandler&& handler =
        asio::default_completion_token_t<Executor>{}
    );

template<class Executor>
class connect_op
{
    using resolver_type = asio::ip::basic_resolver<
        asio::ip::tcp, Executor>;

public:
    using socket_type = 
        asio::basic_socket<asio::ip::tcp, Executor>;

    using endpoint_type = asio::ip::tcp::endpoint;

    connect_op(
        socket_type& sock,
        urls::url_view const& url)
        : sock_(sock)
        , resolver_(sock_.get_executor())
    {
        switch(url.host_type())
        {
        case urls::host_type::none:
            // VFALCO this should probably be an error code
            //BOOST_ASSERT(! url.has_authority());
            throw std::invalid_argument("missing authority");

        case urls::host_type::name:
            host_ = static_cast<std::string>(url.encoded_host());
            scheme_ = static_cast<std::string>(url.scheme());
            break;

        case urls::host_type::ipv4:
        {
            std::uint16_t port;
            if(url.has_port())
                port = url.port_number();
            else
                port = urls::default_port(url.scheme_id());
            if(port == 0)
                // VFALCO this should probably be an error code
                throw std::invalid_argument("invalid port");
            ep_ = { asio::ip::make_address_v4(
                url.host_ipv4_address().to_bytes()), port };
            break;
        }

        case urls::host_type::ipv6:
        {
            std::uint16_t port;
            if(url.has_port())
                port = url.port_number();
            else
                port = urls::default_port(url.scheme_id());
            if(port == 0)
                // VFALCO this should probably be an error code
                throw std::invalid_argument("invalid port");
            ep_ = { asio::ip::make_address_v6(
                url.host_ipv6_address().to_bytes()), port };
            break;
        }

        case urls::host_type::ipvfuture:
            // VFALCO This should probably be an error code
            throw std::invalid_argument("unsupported IPvFuture");
        }
    }

    template<class Self>
    void operator()(Self& self)
    {
        if(! host_.empty())
            return resolver_.async_resolve(
                host_, scheme_, std::move(self));
        sock_.async_connect(ep_, std::move(self));
    }

    template<class Self>
    void operator()(
        Self& self,
        system::error_code const& ec)
    {
        self.complete(ec, ep_);
    }

    template<class Self>
    void operator()(
        Self& self,
        system::error_code const& ec,
        typename resolver_type::results_type results)
    {
        if(ec.failed())
        {
            self.complete(ec, endpoint_type{});
            return;
        }

        asio::async_connect(
            sock_,
            results.begin(),
            results.end(),
            std::move(self));
    }

    template<class Self>
    void operator()(
        Self& self,
        system::error_code const& ec,
        typename resolver_type::results_type::const_iterator it)
    {
        self.complete(ec, *it);
    }

private:
    socket_type& sock_;
    resolver_type resolver_;
    std::string host_;
    std::string scheme_;
    endpoint_type ep_;
};

template<
    class Executor,
    BOOST_ASIO_COMPLETION_TOKEN_FOR(void(
        system::error_code,
        asio::ip::tcp::endpoint)) CompletionToken>
BOOST_ASIO_INITFN_AUTO_RESULT_TYPE(CompletionToken, void(
    system::error_code,
    asio::ip::tcp::endpoint))
async_connect(
    asio::basic_stream_socket<asio::ip::tcp, Executor>& sock,
    urls::url_view const& url,
    CompletionToken&& token)
{
    return asio::async_compose<
        CompletionToken,
        void(
            system::error_code,
            asio::ip::tcp::endpoint)>(
                connect_op<Executor>(sock, url),
                token,
                sock);
}

} // boost

//------------------------------------------------

struct worker
{
    using executor_type =
        boost::asio::io_context::executor_type;
    using socket_type = boost::asio::basic_stream_socket<
        boost::asio::ip::tcp, executor_type>;

    socket_type sock;
    boost::http_proto::response_parser pr;
    boost::urls::url_view url;

    explicit
    worker(
        executor_type ex,
        boost::rts::context& ctx)
        : sock(ex)
        , pr(ctx)
    {
        sock.open(boost::asio::ip::tcp::v4());
    }

    void
    do_next()
    {
        do_visit("http://www.boost.org");
    }

    void
    do_visit(boost::urls::url_view url_)
    {
        url = url_;
        do_resolve();
    }

    void
    do_resolve()
    {
        boost::async_connect(
            sock,
            url,
            [&](
                boost::system::error_code ec,
                boost::asio::ip::tcp::endpoint ep)
            {
                (void)ep;
                if(ec.failed())
                {
                    // log (target, ec.message())
                    auto s = ec.message();
                    return do_next();
                }
                do_request();
            });
    }

    void
    do_request()
    {
        boost::http_proto::request req;
        auto path = url.encoded_path();
        req.set_start_line(
            boost::http_proto::method::get,
            path.empty() ? "/" : path,
            boost::http_proto::version::http_1_1);

        do_shutdown();
    }

    void
    do_shutdown()
    {
        boost::system::error_code ec;
        sock.shutdown(socket_type::shutdown_both, ec);
        if(ec.failed())
        {
            // log(ec.message())
            return do_next();
        }
        //do_next();
    }
};

int
main(int argc, char* argv[])
{
    (void)argc;
    (void)argv;

    boost::rts::context ctx;
    boost::http_proto::parser::config_base cfg;
    boost::http_proto::install_parser_service(ctx, cfg);

    boost::asio::io_context ioc;

    worker w(ioc.get_executor(), ctx);

    w.do_next();

    ioc.run();

    return EXIT_SUCCESS;
}
