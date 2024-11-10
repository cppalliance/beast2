//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include "cookie.hpp"

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/buffers.hpp>
#include <boost/http_io.hpp>
#include <boost/http_proto.hpp>
#include <boost/program_options.hpp>
#include <boost/url.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>

#if defined(BOOST_ASIO_HAS_CO_AWAIT)

#include <variant>
#include <optional>

namespace asio       = boost::asio;
namespace buffers    = boost::buffers;
namespace core       = boost::core;
namespace http_io    = boost::http_io;
namespace http_proto = boost::http_proto;
namespace po         = boost::program_options;
namespace ssl        = boost::asio::ssl;
namespace urls       = boost::urls;
namespace ch         = std::chrono;

using error_code   = boost::system::error_code;
using system_error = boost::system::system_error;

#ifdef BOOST_HTTP_PROTO_HAS_ZLIB
inline const bool http_proto_has_zlib = true;
#else
inline const bool http_proto_has_zlib = false;
#endif

core::string_view
mime_type(core::string_view path) noexcept
{
    const auto ext = [&path]
    {
        const auto pos = path.rfind(".");
        if(pos == core::string_view::npos)
            return core::string_view{};
        return path.substr(pos);
    }();

    namespace ug = urls::grammar;
    if(ug::ci_is_equal(ext, ".gif"))  return "image/gif";
    if(ug::ci_is_equal(ext, ".jpg"))  return "image/jpeg";
    if(ug::ci_is_equal(ext, ".jpeg")) return "image/jpeg";
    if(ug::ci_is_equal(ext, ".png"))  return "image/png";
    if(ug::ci_is_equal(ext, ".svg"))  return "image/svg+xml";
    if(ug::ci_is_equal(ext, ".txt"))  return "text/plain";
    if(ug::ci_is_equal(ext, ".htm"))  return "text/html";
    if(ug::ci_is_equal(ext, ".html")) return "text/html";
    if(ug::ci_is_equal(ext, ".pdf"))  return "application/pdf";
    if(ug::ci_is_equal(ext, ".xml"))  return "application/xml";
    return "application/octet-stream";
}

core::string_view
filename(core::string_view path) noexcept
{
    const auto pos = path.find_last_of("/\\");
    if((pos != std::string_view::npos))
        return path.substr(pos + 1);
    return path;
}

std::uint64_t
filesize(core::string_view path)
{
    http_proto::file file;
    boost::system::error_code ec;

    file.open(
        std::string{ path }.c_str(),
        http_proto::file_mode::scan,
        ec);
    if(ec)
        throw system_error{ ec };

    const auto size = file.size(ec);
    if(ec)
        throw system_error{ ec };

    return size;
}

core::string_view
target(urls::url_view url) noexcept
{
    if(url.encoded_target().empty())
        return "/";

    return url.encoded_target();
}

core::string_view
effective_port(urls::url_view url)
{
    if(url.has_port())
        return url.port();

    if(url.scheme_id() == urls::scheme::https)
        return "443";

    if(url.scheme_id() ==  urls::scheme::http)
        return "80";

    throw std::runtime_error{
        "Unsupported scheme" };
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

void
base64_encode(std::string& dest, core::string_view src)
{
    // Adapted from Boost.Beast project
    char const* in = static_cast<char const*>(src.data());
    static char constexpr tab[] = {
        "ABCDEFGHIJKLMNOP"
        "QRSTUVWXYZabcdef"
        "ghijklmnopqrstuv"
        "wxyz0123456789+/"
    };

    for(auto n = src.size() / 3; n--;)
    {
        dest.append({
            tab[(in[0] & 0xfc) >> 2],
            tab[((in[0] & 0x03) << 4) + ((in[1] & 0xf0) >> 4)],
            tab[((in[2] & 0xc0) >> 6) + ((in[1] & 0x0f) << 2)],
            tab[in[2] & 0x3f] });
        in += 3;
    }

    switch(src.size() % 3)
    {
    case 2:
        dest.append({
            tab[ (in[0] & 0xfc) >> 2],
            tab[((in[0] & 0x03) << 4) + ((in[1] & 0xf0) >> 4)],
            tab[                         (in[1] & 0x0f) << 2],
            '=' });
        break;
    case 1:
        dest.append({
            tab[ (in[0] & 0xfc) >> 2],
            tab[((in[0] & 0x03) << 4)],
            '=',
            '=' });
        break;
    case 0:
        break;
    }
}

class any_stream
{
public:
    using executor_type     = asio::any_io_executor;
    using plain_stream_type = asio::ip::tcp::socket;
    using ssl_stream_type   = ssl::stream<plain_stream_type>;

    any_stream(plain_stream_type stream)
        : stream_{ std::move(stream) }
    {
    }

    any_stream(ssl_stream_type stream)
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
            void(error_code, size_t)>(
            [this, buffers, init = false](
                auto&& self,
                error_code ec = {},
                size_t n      = 0) mutable
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
            void(error_code, size_t)>(
            [this, buffers, init = false](
                auto&& self,
                error_code ec = {},
                size_t n      = 0) mutable
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
            async_compose<CompletionToken, void(error_code)>(
                [this, init = false](
                    auto&& self, error_code ec = {}) mutable
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

class any_ostream
{
    std::variant<std::ofstream, std::ostream*> stream_;

public:
    any_ostream(core::string_view path)
    {
        if(path == "-")
        {
            stream_.emplace<std::ostream*>(&std::cout);
        }
        else if(path == "%")
        {
            stream_.emplace<std::ostream*>(&std::cerr);
        }
        else
        {
            auto& f= stream_.emplace<std::ofstream>();
            f.exceptions(std::ofstream::failbit);
            f.open(path);
        }
    }

    template <typename T>
    any_ostream& operator<<(const T& data) {
        std::visit(
            [&](auto& s)
            {
                if constexpr(std::is_same_v<decltype(s), std::ofstream&>)
                    s << data;
                else
                    *s << data;
            },
            stream_);
        return *this;
    }
};

class urlencoded_form
{
    std::string body_;

public:
    class source;
    void
    append_text(
        core::string_view name,
        core::string_view value) noexcept
    {
        if(!body_.empty())
            body_ += '&';
        body_ += name;
        if(!value.empty())
            body_ += '=';
        append_encoded(value);
    }

    void
    append_file(core::string_view path)
    {
        http_proto::file file;
        error_code ec;

        file.open(
            std::string{ path }.c_str(),
            http_proto::file_mode::read,
            ec);
        if(ec)
            throw system_error{ ec };

        if(!body_.empty())
            body_ += '&';

        for(;;)
        {
            char buf[64 * 1024];
            const auto read = file.read(buf, sizeof(buf), ec);
            if(ec)
                throw system_error{ ec };
            if(read == 0)
                break;
            append_encoded({ buf, read });
        }
    }

    core::string_view
    content_type() const noexcept
    {
        return "application/x-www-form-urlencoded";
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

private:
    void
    append_encoded(core::string_view sv)
    {
        urls::encoding_opts opt;
        opt.space_as_plus = true;
        urls::encode(
            sv,
            urls::pchars,
            opt,
            urls::string_token::append_to(body_));
    }
};

class multipart_form
{
    struct part_t
    {
        core::string_view name;
        core::string_view value_or_path;
        core::string_view content_type;
        std::optional<std::uint64_t> file_size;
    };

    // boundary with extra "--" prefix and postfix.
    std::array<char, 2 + 46 + 2> storage_{ generate_boundary() };
    std::vector<part_t> parts_;

    static constexpr core::string_view content_disposition_ =
        "\r\nContent-Disposition: form-data; name=\"";
    static constexpr core::string_view filename_ =
        "; filename=\"";
    static constexpr core::string_view content_type_ =
        "\r\nContent-Type: ";

public:
    class source;

    void
    append_text(
        core::string_view name,
        core::string_view value,
        core::string_view content_type)
    {
        parts_.emplace_back(name, value, content_type );
    }

    void
    append_file(
        core::string_view name,
        core::string_view path,
        core::string_view content_type)
    {
        // store size because file may change on disk between
        // call to content_length and serialization.
        parts_.emplace_back(
            name, path, content_type, filesize(path));
    }

    std::string
    content_type() const noexcept
    {
        std::string res = "multipart/form-data; boundary=";
        // append boundary
        res.append(storage_.begin() + 2, storage_.end() - 2);
        return res;
    }

    std::uint64_t
    content_length() const noexcept
    {
        auto rs = std::uint64_t{};
        for(const auto& part : parts_)
        {
            rs += storage_.size() - 2; // --boundary
            rs += content_disposition_.size();
            rs += part.name.size();
            rs += 1; // closing double quote

            if(!part.content_type.empty())
            {
                rs += content_type_.size();
                rs += part.content_type.size();
            }

            if(part.file_size.has_value()) // file
            {
                rs += filename_.size();
                rs += filename(part.value_or_path).size();
                rs += 1; // closing double quote
                rs += part.file_size.value();
            }
            else // text
            {
                rs += part.value_or_path.size();
            }

            rs += 4; // <CRLF><CRLF> after header
            rs += 2; // <CRLF> after content
        }
        rs += storage_.size(); // --boundary--
        return rs;
    }

private:
    static
    decltype(storage_)
    generate_boundary()
    {
        decltype(storage_) rs;
        constexpr static char chars[] =
            "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
        static std::random_device rd;
        std::uniform_int_distribution<int> dist{ 0, sizeof(chars) - 2 };
        std::fill(rs.begin(), rs.end(), '-');
        std::generate(
            rs.begin() + 2 + 24,
            rs.end() - 2,
            [&] {  return chars[dist(rd)]; });
        return rs;
    }
};

class multipart_form::source
    : public http_proto::source
{
    const multipart_form* form_;
    std::vector<part_t>::const_iterator it_{ form_->parts_.begin() };
    int step_           = 0;
    std::uint64_t skip_ = 0;

public:
    explicit source(const multipart_form* form) noexcept
        : form_{ form }
    {
    }

    results
    on_read(buffers::mutable_buffer mb) override
    {
        auto rs = results{};

        auto copy = [&](core::string_view sv)
        {
            auto copied = buffers::buffer_copy(
                mb,
                buffers::sans_prefix(
                    buffers::const_buffer{ sv.data(), sv.size() },
                    static_cast<std::size_t>(skip_)));

            mb = buffers::sans_prefix(mb, copied);
            rs.bytes += copied;
            skip_    += copied;

            if(skip_ != sv.size())
                return false;

            skip_ = 0;
            return true;
        };

        auto read = [&](core::string_view path, uint64_t size)
        {
            http_proto::file file;

            file.open(
                std::string{ path }.c_str(),
                http_proto::file_mode::read,
                rs.ec);
            if(rs.ec)
                return false;

            file.seek(skip_, rs.ec);
            if(rs.ec)
                return false;

            auto read = file.read(
                mb.data(),
                (std::min)(static_cast<
                    std::uint64_t>(mb.size()), size),
                rs.ec);
            if(rs.ec)
                return false;

            mb = buffers::sans_prefix(mb, read);
            rs.bytes += read;
            skip_    += read;

            if(skip_ != size)
                return false;

            skip_ = 0;
            return true;
        };

        while(it_ != form_->parts_.end())
        {
            switch(step_)
            {
            case 0:
                // --boundary
                if(!copy({ form_->storage_.data(),
                    form_->storage_.size() - 2 })) return rs;
                ++step_;
            case 1:
                if(!copy(content_disposition_)) return rs;
                ++step_;
            case 2:
                if(!copy(it_->name)) return rs;
                ++step_;
            case 3:
                if(!copy("\"")) return rs;
                ++step_;
            case 4:
                if(!it_->file_size.has_value())
                    goto content_type;
                if(!copy(filename_)) return rs;
                ++step_;
            case 5:
                if(!copy(filename(it_->value_or_path))) return rs;
                ++step_;
            case 6:
                if(!copy("\"")) return rs;
                ++step_;
            case 7:
            content_type:
                if(it_->content_type.empty())
                    goto end_of_header;
                if(!copy(content_type_)) return rs;
                ++step_;
            case 8:
                if(!copy(it_->content_type)) return rs;
                ++step_;
            case 9:
            end_of_header:
                if(!copy("\r\n\r\n")) return rs;
                ++step_;
            case 10:
                if(it_->file_size)
                {
                    if(!read(
                        it_->value_or_path,
                        it_->file_size.value())) return rs;
                }
                else
                {
                    if(!copy(it_->value_or_path)) return rs;
                }
                ++step_;
            case 11:
                if(!copy("\r\n"))
                    return rs;
                step_ = 0;
                ++it_;
            }
        }

        // --boundary--
        if(!copy({ form_->storage_.data(),
            form_->storage_.size() })) return rs;

        rs.finished = true;
        return rs;
    };
};

class json_body
{
    std::string body_;

public:
    class source;
    void
    append_text(core::string_view value) noexcept
    {
        body_.append(value);
    }

    void
    append_file(core::string_view path)
    {
        http_proto::file file;
        error_code ec;

        file.open(
            std::string{ path }.c_str(),
            http_proto::file_mode::read,
            ec);
        if(ec)
            throw system_error{ ec };

        for(;;)
        {
            char buf[64 * 1024];
            const auto read = file.read(buf, sizeof(buf), ec);
            if(ec)
                throw system_error{ ec };
            if(read == 0)
                break;
            body_.append(buf, read);
        }
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

asio::awaitable<void>
connect_socks5_proxy(
    asio::ip::tcp::socket& stream,
    urls::url_view url,
    urls::url_view proxy)
{
    auto executor = co_await asio::this_coro::executor;
    auto resolver = asio::ip::tcp::resolver{ executor };
    auto rresults = co_await resolver.async_resolve(
        proxy.host(), effective_port(proxy));

    // Connect to the proxy server
    co_await asio::async_connect(stream, rresults);

    // Greeting request
    if(proxy.has_userinfo())
    {
        std::uint8_t greeting_req[4] = { 0x05, 0x02, 0x00, 0x02 };
        co_await asio::async_write(stream, asio::buffer(greeting_req));
    }
    else
    {
        std::uint8_t greeting_req[3] = { 0x05, 0x01, 0x00 };
        co_await asio::async_write(stream, asio::buffer(greeting_req));
    }

    // Greeting response
    std::uint8_t greeting_resp[2];
    co_await asio::async_read(stream, asio::buffer(greeting_resp));

    if(greeting_resp[0] != 0x05)
        throw std::runtime_error{ "SOCKS5 invalid version" };

    switch(greeting_resp[1])
    {
    case 0x00: // No Authentication
        break;
    case 0x02: // Username/password
    {
        // Authentication request
        auto auth_req = std::string{ 0x01 };

        auto user = proxy.encoded_user();
        auth_req.push_back(static_cast<std::uint8_t>(user.decoded_size()));
        user.decode({}, urls::string_token::append_to(auth_req));

        auto pass = proxy.encoded_password();
        auth_req.push_back(static_cast<std::uint8_t>(pass.decoded_size()));
        pass.decode({}, urls::string_token::append_to(auth_req));

        co_await asio::async_write(stream, asio::buffer(auth_req));

        // Authentication response
        std::uint8_t greeting_resp[2];
        co_await asio::async_read(stream, asio::buffer(greeting_resp));

        if(greeting_resp[1] != 0x00)
            throw std::runtime_error{
                "SOCKS5 authentication failed" };
        break;
    }
    default:
        throw std::runtime_error{
            "SOCKS5 no acceptable authentication method"
        };
    }

    // Connection request
    auto conn_req = std::string{ 0x05, 0x01, 0x00, 0x03 };
    auto host     = url.encoded_host();
    conn_req.push_back(static_cast<std::uint8_t>(host.decoded_size()));
    host.decode({}, urls::string_token::append_to(conn_req));

    std::uint16_t port = std::stoi(effective_port(url));
    conn_req.push_back(static_cast<std::uint8_t>((port >> 8) & 0xFF));
    conn_req.push_back(static_cast<std::uint8_t>(port & 0xFF));

    co_await asio::async_write(stream, asio::buffer(conn_req));

    // Connection response
    std::uint8_t conn_resp_head[5];
    co_await asio::async_read(stream, asio::buffer(conn_resp_head));

    if(conn_resp_head[1] != 0x00)
        throw std::runtime_error{
            "SOCKS5 connection request failed" };

    std::string conn_resp_tail;
    conn_resp_tail.resize(
        [&]()
        {
            // subtract 1 because we have pre-read one byte
            switch(conn_resp_head[3])
            {
            case 0x01:
                return 4 + 2 - 1; // ipv4 + port
            case 0x03:
                return conn_resp_head[4] + 2 - 1; // domain name + port
            case 0x04:
                return 16 + 2 - 1; // ipv6 + port
            default:
                throw std::runtime_error{
                    "SOCKS5 invalid address type" };
            }
        }()); 
    co_await asio::async_read(stream, asio::buffer(conn_resp_tail));
}

asio::awaitable<void>
connect_http_proxy(
    const po::variables_map& vm,
    http_proto::context& http_proto_ctx,
    asio::ip::tcp::socket& stream,
    urls::url_view url,
    urls::url_view proxy)
{
    auto executor = co_await asio::this_coro::executor;
    auto resolver = asio::ip::tcp::resolver{ executor };
    auto rresults = co_await resolver.async_resolve(
        proxy.host(), effective_port(proxy));

    // Connect to the proxy server
    co_await asio::async_connect(stream, rresults);

    using field    = http_proto::field;
    auto request   = http_proto::request{};
    auto host_port = [&]()
    {
        auto rs = url.encoded_host().decode();
        rs.push_back(':');
        rs.append(effective_port(url));
        return rs;
    }();

    request.set_method(http_proto::method::connect);
    request.set_target(host_port);
    request.set(field::host, host_port);
    request.set(field::proxy_connection, "keep-alive");

    if(vm.count("user-agent"))
    {
        request.set(
            field::user_agent,
            vm.at("user-agent").as<std::string>());
    }
    else
    {
        request.set(field::user_agent, "Boost.Http.Io");
    }

    if(proxy.has_userinfo())
    {
        auto credentials = proxy.encoded_userinfo().decode();
        auto basic_auth  = std::string{ "Basic " };
        base64_encode(basic_auth, credentials);
        request.set(field::proxy_authorization, basic_auth);
    }

    auto serializer = http_proto::serializer{ http_proto_ctx };
    auto parser     = http_proto::response_parser{ http_proto_ctx };

    serializer.start(request);
    co_await http_io::async_write(stream, serializer);

    parser.reset();
    parser.start();
    co_await http_io::async_read_header(stream, parser);

    if(parser.get().status() != http_proto::status::ok)
        throw std::runtime_error{
            "Proxy server rejected the connection" };
}

asio::awaitable<void>
connect(
    const po::variables_map& vm,
    ssl::context& ssl_ctx,
    http_proto::context& http_proto_ctx,
    any_stream& stream,
    urls::url_view url)
{
    auto executor = co_await asio::this_coro::executor;
    auto socket   = asio::ip::tcp::socket{ executor };

    if(vm.count("proxy"))
    {
        auto proxy_url = urls::parse_uri(vm.at("proxy").as<std::string>());

        if(proxy_url.has_error())
            throw system_error{ proxy_url.error(), "Failed to parse proxy" };

        if(proxy_url->scheme() == "http")
        {
            co_await connect_http_proxy(
                vm, http_proto_ctx, socket, url, proxy_url.value());
        }
        else if(proxy_url->scheme() == "socks5")
        {
            co_await connect_socks5_proxy(
                socket, url, proxy_url.value());
        }
        else
        {
            throw std::runtime_error{
                "only HTTP and SOCKS5 proxies are supported" };
        }
    }
    else // no proxy
    {
        auto resolver = asio::ip::tcp::resolver{ executor };
        auto rresults = co_await resolver.async_resolve(
            url.host(), effective_port(url));
        co_await asio::async_connect(socket, rresults);
    }

    if(vm.count("tcp-nodelay"))
        socket.set_option(asio::ip::tcp::no_delay{ true });

    if(vm.count("no-keepalive"))
        socket.set_option(asio::ip::tcp::socket::keep_alive{ false });

    // TLS handshake
    if(url.scheme_id() == urls::scheme::https)
    {
        auto ssl_stream = ssl::stream<asio::ip::tcp::socket>{
            std::move(socket), ssl_ctx };

        auto host = std::string{ url.host() };
        if(!SSL_set_tlsext_host_name(
            ssl_stream.native_handle(), host.c_str()))
        {
            throw system_error{ static_cast<int>(::ERR_get_error()),
                asio::error::get_ssl_category() };
        }

        co_await ssl_stream.async_handshake(ssl::stream_base::client);
        stream = std::move(ssl_stream);
        co_return ;
    }

    stream = std::move(socket);
}

http_proto::request
create_request(
    const po::variables_map& vm,
    const message& msg,
    urls::url_view url)
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

    if(vm.count("user"))
    {
        auto credentials = vm.at("user").as<std::string>();
        auto basic_auth  = std::string{ "Basic " };
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
    urls::url_view url)
{
    using field     = http_proto::field;
    auto executor   = co_await asio::this_coro::executor;
    auto parser     = http_proto::response_parser{ http_proto_ctx };
    auto serializer = http_proto::serializer{ http_proto_ctx };
    auto stream     = any_stream{ asio::ip::tcp::socket{ executor } };

    auto connect_to = [&](const urls::url_view& url)
    {
        auto timeout = vm.count("connect-timeout")
            ? ch::duration_cast<ch::steady_clock::duration>(
                ch::duration<float>(vm.at("connect-timeout").as<float>()))
            : ch::steady_clock::duration::max();

        return asio::co_spawn(
            executor,
            connect(vm, ssl_ctx, http_proto_ctx, stream, url),
            asio::cancel_after(timeout));
    };

    auto set_cookies = [&](urls::url_view url)
    {
        auto field = cookie_jar ? cookie_jar->make_field(url) : std::string{};
        field.append(explicit_cookies);
        if(!field.empty())
            request.set(field::cookie, field);
    };

    auto extract_cookies = [&](urls::url_view url)
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
            urls::url location = urls::parse_uri(it->value).value();

            if(!can_reuse_connection(response, referer, location))
            {
                if(!vm.count("proxy"))
                    co_await stream.async_shutdown(asio::as_tuple);

                co_await connect_to(location);
            }
            else
            {
                // Consume the body
                if(request.method() != http_proto::method::head)
                    co_await http_io::async_read(stream, parser);
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
            request.set(field::referer, location);

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

        auto url = urls::parse_uri(vm.at("url").as<std::string>());
        if(url.has_error())
            throw system_error{ url.error(), "Failed to parse URL" };

        auto ioc            = asio::io_context{};
        auto ssl_ctx        = ssl::context{ ssl::context::tlsv12_client };
        auto http_proto_ctx = http_proto::context{};

        if(vm.count("insecure"))
        {
            ssl_ctx.set_verify_mode(ssl::verify_none);
        }
        else
        {
            ssl_ctx.set_default_verify_paths();
            ssl_ctx.set_verify_mode(ssl::verify_peer);
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
                            mime_type(value.substr(1)));
                    }
                    else
                    {
                        form.append_text(name, value, "");
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
            auto form = urlencoded_form{};
            for(auto& data : vm.at("data").as<std::vector<std::string>>())
            {
                if(!data.empty() && data[0] == '@')
                {
                    form.append_file(data.substr(1));
                }
                else
                {
                    if(auto pos = data.find('=');
                        pos != std::string::npos)
                    {
                        form.append_text(
                            data.substr(0, pos),
                            data.substr(pos + 1));
                    }
                    else
                    {
                        form.append_text(data, "");
                    }
                }
            }
            msg = std::move(form);
        }

        if(vm.count("json"))
        {
            auto body = json_body{};
            for(auto& data : vm.at("json").as<std::vector<std::string>>())
            {
                if(!data.empty() && data[0] == '@')
                {
                    body.append_file(data.substr(1));
                }
                else
                {
                    body.append_text(data);
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
                if(option.find('=') != std::string::npos)
                {
                    if(!explicit_cookies.ends_with(';'))
                        explicit_cookies.push_back(';');
                    explicit_cookies.append(option);
                }
                else
                {
                    auto ifs = std::ifstream{ option };
                    ifs.exceptions(std::ifstream::badbit);
                    ifs >> cookie_jar.value();
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
                create_request(vm, msg, url.value()),
                url.value()),
            [](std::exception_ptr ep)
            {
                if(ep)
                    std::rethrow_exception(ep);
            });

        ioc.run();

        if(vm.count("cookie-jar"))
        {
            auto s = any_ostream{ vm.at("cookie-jar").as<std::string>() };
            s << cookie_jar.value();
        }
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
