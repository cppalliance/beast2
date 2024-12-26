//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include "options.hpp"
#include "any_iostream.hpp"
#include "glob.hpp"
#include "mime_type.hpp"

#include <boost/asio/ssl.hpp>
#include <boost/config.hpp>
#include <boost/program_options.hpp>
#include <boost/url.hpp>

namespace grammar  = urls::grammar;
namespace po       = boost::program_options;
using system_error = boost::system::system_error;

namespace
{
core::string_view
effective_port(const urls::url_view& url)
{
    if(url.has_port())
        return url.port();

    if(url.scheme() == "https")
        return "443";

    if(url.scheme() == "http")
        return "80";

    if(url.scheme() == "socks5")
        return "1080";

    throw std::runtime_error{ "Unsupported scheme" };
}

struct quoted_string_t
{
    using value_type = core::string_view;

    constexpr boost::system::result<value_type>
    parse(char const*& it, char const* end) const noexcept
    {
        const auto it0 = it;

        if(it == end)
            return grammar::error::need_more;

        if(*it++ != '"')
            return grammar::error::mismatch;

        for(; it != end && *it != '"'; it++)
        {
            if(*it == '\\')
                it++;
        }

        if(*it != '"')
            return grammar::error::mismatch;

        return value_type(it0, ++it);
    }
};

constexpr auto quoted_string = quoted_string_t{};

std::string
unquote_string(core::string_view sv)
{
    sv.remove_prefix(1);
    sv.remove_suffix(1);

    auto rs = std::string{};
    for(auto it = sv.begin(); it != sv.end(); it++)
    {
        if(*it == '\\')
            it++;
        rs.push_back(*it);
    }
    return rs;
}

struct form_option_result
{
    std::string name;
    char prefix = '\0';
    std::string value;
    boost::optional<std::string> filename;
    boost::optional<std::string> type;
    std::vector<std::string> headers;
};

form_option_result
parse_form_option(core::string_view sv)
{
    constexpr auto name_rule =
        grammar::token_rule(grammar::all_chars - grammar::lut_chars('='));

    constexpr auto value_rule = grammar::variant_rule(
        quoted_string,
        grammar::token_rule(grammar::all_chars - grammar::lut_chars(';')));

    static constexpr auto parser = grammar::tuple_rule(
        name_rule,
        grammar::squelch(grammar::delim_rule('=')),
        grammar::optional_rule(grammar::delim_rule(grammar::lut_chars("@<"))),
        value_rule,
        grammar::range_rule(
            grammar::tuple_rule(
                grammar::squelch(grammar::delim_rule(';')),
                name_rule,
                grammar::squelch(grammar::delim_rule('=')),
                value_rule)));

    const auto parse_rs = grammar::parse(sv, parser);

    if(parse_rs.has_error())
        throw std::runtime_error{ "Illegally formatted input field" };

    auto [name, prefix, value, range] = parse_rs.value();

    auto extract = [](decltype(value_rule)::value_type v)
    {
        if(v.index() == 0)
            return unquote_string(get<0>(v));

        return std::string{ get<1>(v) };
    };

    form_option_result rs;

    rs.name   = name;
    rs.value  = extract(value);
    rs.prefix = prefix.value_or(" ").at(0);

    for(auto [att_name, att_value] : range)
    {
        if(att_name == "filename")
            rs.filename = extract(att_value);
        else if(att_name == "type")
            rs.type = extract(att_value);
        else if(att_name == "headers")
            rs.headers.push_back(extract(att_value));
        else
            throw std::runtime_error{ "Illegally formatted input field" };
    }

    return rs;
}

boost::system::result<std::uint64_t>
parse_human_readable_size(core::string_view sv)
{
    struct ci_delim_rule
    {
        using value_type = char;

        constexpr ci_delim_rule(char ch) noexcept
            : ch_(grammar::to_lower(ch))
        {
        }

        constexpr boost::system::result<value_type>
        parse(char const*& it, char const* end) const noexcept
        {
            if(it == end)
                return grammar::error::need_more;

            if(grammar::to_lower(*it++) != ch_)
                return grammar::error::mismatch;

            return ch_;
        }

    private:
        char ch_;
    };

    static constexpr auto parser = grammar::tuple_rule(
        grammar::token_rule(grammar::digit_chars + grammar::lut_chars(".")),
        grammar::optional_rule(
            grammar::variant_rule(
                ci_delim_rule('B'),
                ci_delim_rule('K'),
                ci_delim_rule('M'),
                ci_delim_rule('G'),
                ci_delim_rule('T'),
                ci_delim_rule('P'))));
    const auto parse_rs = grammar::parse(sv, parser);

    if(parse_rs.has_error())
        return parse_rs.error();

    auto [size, unit] = parse_rs.value();

    // TODO: prevent overflow
    // TODO: replace std::stod
    return static_cast<std::uint64_t>(
        std::stod(size) * (1ULL << 10 * unit.value_or(char{}).index()));
}

#ifdef BOOST_WINDOWS
#include <wincrypt.h>

void
load_windows_root_certs(asio::ssl::context& ctx)
{
    HCERTSTORE h_store = CertOpenSystemStore(0, "ROOT");
    if(!h_store)
        throw std::runtime_error(
            "Failed to open the \"ROOT\" system certificate store");

    X509_STORE* x_store      = X509_STORE_new();
    PCCERT_CONTEXT p_context = nullptr;
    while((p_context = CertEnumCertificatesInStore(h_store, p_context)))
    {
        const unsigned char* in = p_context->pbCertEncoded;
        X509* x509 = d2i_X509(nullptr, &in, p_context->cbCertEncoded);
        if(x509)
        {
            X509_STORE_add_cert(x_store, x509);
            X509_free(x509);
        }
    }

    CertFreeCertificateContext(p_context);
    CertCloseStore(h_store, 0);

    SSL_CTX_set_cert_store(ctx.native_handle(), x_store);
}
#endif
} // namespace

parse_args_result
parse_args(int argc, char* argv[])
{
    auto odesc = po::options_description{ "Options" };
    // clang-format off
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
            po::value<double>()->value_name("<frac sec>"),
            "Maximum time allowed for connection")
        ("connect-to",
            po::value<std::vector<std::string>>()->value_name("<H1:P1:H2:P2>"),
            "Connect to host")
        ("continue-at,C",
            po::value<std::string>()->value_name("<offset>"),
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
            po::value<double>()->value_name("<frac sec>"),
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
        ("globoff,g", "Disable URL sequences and ranges using {} and []")
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
        ("limit-rate",
            po::value<std::string>()->value_name("<speed>"),
            "Limit transfer speed to RATE")
        ("location,L", "Follow redirects")
        ("location-trusted", "Like --location, and send auth to other hosts")
        ("max-filesize",
            po::value<std::string>()->value_name("<bytes>"),
            "Maximum file size to download")
        ("max-redirs",
            po::value<std::int64_t>()->value_name("<num>"),
            "Maximum number of redirects allowed")
        ("max-time",
            po::value<double>()->value_name("<frac sec>"),
            "Maximum time allowed for transfer")
        ("no-buffer", "Disable buffering of the output stream")
        ("no-keepalive", "Disable TCP keepalive on the connection")
        ("no-progress-meter", "Do not show the progress meter")
        ("output,o",
            po::value<std::vector<std::string>>()->value_name("<file>"),
            "Write to file instead of stdout")
        ("output-dir",
            po::value<std::string>()->value_name("<dir>"),
            "Directory to save files in")
        ("parallel,Z", "Perform transfers in parallel")
        ("parallel-immediate", "Do not wait for multiplexing (with --parallel)")
        ("parallel-max",
            po::value<std::uint16_t>()->value_name("<num>"),
            "Maximum concurrency for parallel transfers")
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
        ("remote-name,O",
            po::value<std::vector<bool>>()->zero_tokens(),
            "Write output to a file named as the remote file")
        ("remote-name-all", "Use the remote file name for all URLs")
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
        ("retry",
            po::value<std::uint64_t>()->value_name("<num>"),
            "Retry request if transient problems occur")
        ("retry-all-errors", "Retry all errors (use with --retry)")
        ("retry-connrefused", "Retry on connection refused (use with --retry)")
        ("retry-delay",
            po::value<double>()->value_name("<frac sec>"),
            "Wait time between retries")
        ("retry-max-time",
            po::value<double>()->value_name("<frac sec>"),
            "Retry only within this period")
        ("show-headers", "Show response headers in the output")
        ("skip-existing", "Skip download if local file already exists")
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
            po::value<std::vector<std::string>>()->value_name("<file>"),
            "Transfer local FILE to destination")
        ("url",
            po::value<std::vector<std::string>>()->value_name("<url>"),
            "URL to work with")
        ("url-query",
            po::value<std::vector<std::string>>()->value_name("<data>"),
            "Add a URL query part")
        ("user,u",
            po::value<std::string>()->value_name("<user:password>"),
            "Server user and password")
        ("user-agent,A",
            po::value<std::string>()->value_name("<name>"),
            "Send User-Agent <name> to server");
    // clang-format on

    auto podesc = po::positional_options_description{};
    podesc.add("url", -1);

    auto vm = po::variables_map{};
    auto po = po::command_line_parser{ argc, argv }
                  .options(odesc)
                  .positional(podesc)
                  .run();
    po::store(po, vm);
    po::notify(vm);

    if(vm.contains("help") || !vm.contains("url"))
    {
        std::stringstream ss;
        // clang-format off
        ss << "Usage: burl [options...] <url>\n"
           << "Example:\n"
           << "    burl https://www.example.com\n"
           << "    burl -L http://httpstat.us/301\n"
           << "    burl https://httpbin.org/post -F name=Shadi -F img=@./avatar.jpeg\n"
           << "    burl \"http://example.com/archive/vol[1-4]/part{a,b,c}.html\" -o vol#1-part#2\n"
           << "    burl https://archives.boost.io/release/1.87.0/source/boost_1_87_0.zip -OLC -\n"
           << odesc;
        // clang-format on
        throw std::runtime_error{ ss.str() };
    }

    auto oc = operation_config{};

    auto set_frac_sec = [&](auto& out, core::string_view option)
    {
        if(vm.contains(option))
            out = std::chrono::duration_cast<operation_config::duration_type>(
                std::chrono::duration<double>(vm.at(option).as<double>()));
    };

    auto set_bool = [&](bool& out, core::string_view option)
    {
        if(vm.contains(option))
            out |= true;
    };

    auto set_string = [&](auto& out, core::string_view option)
    {
        // effectively an OR operation
        if(vm.contains(option))
            out = vm.at(option).as<std::string>();
    };

    set_frac_sec(oc.timeout, "max-time");
    set_frac_sec(oc.connect_timeout, "connect-timeout");
    set_frac_sec(oc.expect100timeout, "expect100-timeout");
    set_frac_sec(oc.retry_delay, "retry-delay");
    set_frac_sec(oc.retry_maxtime, "retry-max-time");

    set_bool(oc.disallow_username_in_url, "disallow-username-in-url");
    set_bool(oc.encoding, "compressed");
    set_bool(oc.create_dirs, "create-dirs");
    set_bool(oc.tcp_nodelay, "tcp-nodelay");
    set_bool(oc.retry_all_errors, "retry-all-errors");
    set_bool(oc.retry_connrefused, "retry-connrefused");
    set_bool(oc.nokeepalive, "no-keepalive");
    set_bool(oc.post301, "post301");
    set_bool(oc.post302, "post302");
    set_bool(oc.post303, "post303");
    set_bool(oc.http10, "http1.0");
    set_bool(oc.ipv4, "ipv4");
    set_bool(oc.ipv6, "ipv6");
    set_bool(oc.failonerror, "fail");
    set_bool(oc.failwithbody, "fail-with-body");
    set_bool(oc.rm_partial, "remove-on-error");
    set_bool(oc.use_httpget, "get");
    set_bool(oc.show_headers, "show-headers");
    set_bool(oc.show_headers, "include");
    set_bool(oc.show_headers, "head");
    set_bool(oc.no_body, "head");
    set_bool(oc.cookiesession, "junk-session-cookies");
    set_bool(oc.content_disposition, "remote-header-name");
    set_bool(oc.followlocation, "location");
    set_bool(oc.followlocation, "location-trusted");
    set_bool(oc.unrestricted_auth, "location-trusted");
    set_bool(oc.nobuffer, "no-buffer");
    set_bool(oc.globoff, "globoff");
    set_bool(oc.noprogress, "no-progress-meter");
    set_bool(oc.skip_existing, "skip-existing");

    set_string(oc.unix_socket_path, "unix-socket");
    set_string(oc.useragent, "user-agent");
    set_string(oc.userpwd, "user");
    set_string(oc.range, "range");
    set_string(oc.request_target, "request-target");
    set_string(oc.customrequest, "request");
    set_string(oc.headerfile, "dump-header");
    set_string(oc.range, "range");
    set_string(oc.output_dir, "output-dir");
    set_string(oc.cookiejar, "cookie-jar");

    if(oc.failonerror && oc.failwithbody)
        throw std::runtime_error(
            "You must select either --fail or --fail-with-body, not both.");

    if(oc.show_headers && oc.content_disposition)
        throw std::runtime_error(
            "showing headers and --remote-header-name cannot be combined");

    if(vm.contains("proxy"))
        oc.proxy = urls::url{ vm.at("proxy").as<std::string>() };

    if(vm.contains("retry"))
        oc.req_retry = vm.at("retry").as<std::uint64_t>();

    if(vm.contains("abstract-unix-socket"))
        oc.unix_socket_path =
            '\0' + vm.at("abstract-unix-socket").as<std::string>();

    if(vm.contains("referer"))
    {
        auto referer = vm.at("referer").as<std::string>();
        if(referer == ";auto")
            oc.autoreferer = true;
        else
            oc.referer = urls::url{ referer };
    }

    if(vm.contains("limit-rate"))
    {
        auto limit =
            parse_human_readable_size(vm.at("limit-rate").as<std::string>());
        if(limit.has_error())
            throw std::runtime_error{ "unsupported limit-rate unit" };
        oc.recvpersecond = limit.value();
        oc.sendpersecond = limit.value();
    }

    if(vm.contains("max-redirs"))
    {
        auto value   = vm.at("max-redirs").as<std::int64_t>();
        oc.maxredirs = value < 0 ? std::numeric_limits<std::uint64_t>::max()
                                 : static_cast<std::uint64_t>(value);
    }

    if(vm.contains("max-filesize"))
    {
        auto limit =
            parse_human_readable_size(vm.at("max-filesize").as<std::string>());
        if(limit.has_error())
            throw std::runtime_error{ "unsupported max-filesize unit" };
        oc.max_filesize = limit.value();
    }

    if(vm.contains("parallel"))
        oc.parallel_max = 50; // default

    if(vm.contains("parallel-max"))
    {
        auto value = vm.at("parallel-max").as<std::uint16_t>();
        if(value <= 300)
            oc.parallel_max = value;
    }

    if(vm.contains("proto-redir"))
    {
        oc.proto_redir.clear();
        for(auto& s : vm.at("proto-redir").as<std::vector<std::string>>())
        {
            if(s == "http")
                oc.proto_redir.emplace(urls::scheme::http);
            if(s == "https")
                oc.proto_redir.emplace(urls::scheme::https);
        }
    }

    if(vm.contains("continue-at"))
    {
        if(oc.range)
            throw std::runtime_error(
                "--continue-at is mutually exclusive with --range");

        if(oc.rm_partial)
            throw std::runtime_error(
                "--continue-at is mutually exclusive with --remove-on-error");

        auto s = vm.at("continue-at").as<std::string>();

        if(s == "-")
            oc.resume_from_current = true;
        else
            oc.resume_from = std::stoull(s);
    }

    if(!oc.cookiejar.empty())
        oc.enable_cookies = true;

    if(vm.contains("cookie"))
    {
        // Empty options are allowerd and just activates cookie engine.
        oc.enable_cookies = true;

        // If no "=" symbol is used in the argument, it is instead treated as a
        // filename to read previously stored cookie from.
        for(core::string_view sv :
            vm.at("cookie").as<std::vector<std::string>>())
        {
            if(sv.find('=') != core::string_view::npos)
                oc.cookies.push_back(sv);
            else if(!sv.empty())
                oc.cookiefiles.emplace_back(sv.begin(), sv.end());
        }
    }

    // TLS/SSL configs
    auto ssl_ctx = asio::ssl::context{ asio::ssl::context::tls_client };
    {
        auto tls_min = 11;
        auto tls_max = 13;

        if(vm.contains("tls-max"))
        {
            auto s = vm.at("tls-max").as<std::string>();
            if(s == "1.0")
                tls_max = 10;
            else if(s == "1.1")
                tls_max = 11;
            else if(s == "1.2")
                tls_max = 12;
            else if(s == "1.3")
                tls_max = 13;
            else
                throw std::runtime_error{ "Wrong TLS version" };
        }

        if(vm.contains("tlsv1.0"))
            tls_min = 10;
        if(vm.contains("tlsv1.1"))
            tls_min = 11;
        if(vm.contains("tlsv1.2"))
            tls_min = 12;
        if(vm.contains("tlsv1.3"))
            tls_min = 13;

        if(tls_min > 10)
            ssl_ctx.set_options(ssl_ctx.no_tlsv1);
        if(tls_min > 11 || tls_max < 11)
            ssl_ctx.set_options(ssl_ctx.no_tlsv1_1);
        if(tls_min > 12 || tls_max < 12)
            ssl_ctx.set_options(ssl_ctx.no_tlsv1_2);
        if(tls_max < 13)
            ssl_ctx.set_options(ssl_ctx.no_tlsv1_3);
    }

    if(vm.contains("insecure"))
    {
        ssl_ctx.set_verify_mode(asio::ssl::verify_none);
    }
    else
    {
        if(vm.contains("cacert"))
        {
            ssl_ctx.load_verify_file(vm.at("cacert").as<std::string>());
        }
        else if(vm.contains("capath"))
        {
            ssl_ctx.add_verify_path(vm.at("capath").as<std::string>());
        }
        else
        {
#ifdef BOOST_WINDOWS
            load_windows_root_certs(ssl_ctx);
#endif
            ssl_ctx.set_default_verify_paths();
        }

        ssl_ctx.set_verify_mode(asio::ssl::verify_peer);
    }

    if(vm.contains("cert"))
    {
        ssl_ctx.use_certificate_file(
            vm.at("cert").as<std::string>(),
            asio::ssl::context::file_format::pem);
    }

    if(vm.contains("ciphers"))
    {
        if(::SSL_CTX_set_cipher_list(
               ssl_ctx.native_handle(),
               vm.at("ciphers").as<std::string>().c_str()) != 1)
        {
            throw std::runtime_error{ "failed setting cipher list" };
        }
    }

    if(vm.contains("tls13-ciphers"))
    {
        if(::SSL_CTX_set_ciphersuites(
               ssl_ctx.native_handle(),
               vm.at("tls13-ciphers").as<std::string>().c_str()) != 1)
        {
            throw std::runtime_error{ "failed setting TLS 1.3 cipher suite" };
        }
    }

    if(vm.contains("curves"))
    {
        if(::SSL_CTX_set1_curves_list(
               ssl_ctx.native_handle(),
               vm.at("curves").as<std::string>().c_str()) != 1)
        {
            throw std::runtime_error{ "failed setting curves list" };
        }
    }

    if(vm.contains("pass"))
    {
        ssl_ctx.set_password_callback(
            [&](auto, auto) { return vm.at("pass").as<std::string>(); });
    }

    if(vm.contains("key"))
    {
        ssl_ctx.use_private_key_file(
            vm.at("key").as<std::string>(),
            asio::ssl::context::file_format::pem);
    }

    struct request_info
    {
        std::function<boost::optional<glob_result>()> url_gen;
        std::string output;
        std::string input;
        bool remotename;
    };

    auto requests = std::vector<request_info>{};

    for(auto& s : vm.at("url").as<std::vector<std::string>>())
    {
        // returns url a single time, then empty optional
        auto make_oneshot_gen = [](std::string s)
        {
            return [url = boost::make_optional(std::move(s))]() mutable
            {
                if(!url.has_value())
                    return boost::optional<glob_result>{};
                return boost::optional<glob_result>(
                    { std::exchange(url, {}).value(), {} });
            };
        };

        requests.push_back(
            { oc.globoff ? make_oneshot_gen(s) : make_glob_generator(s),
              {},
              {},
              vm.contains("remote-name-all") });
    }

    // Manually iterating through parsed options to preserve
    // the order of arguments.
    for(auto it = requests.begin(); auto& option : po.options)
    {
        if(it == requests.end())
            break;

        if(option.string_key == "output")
            it++->output = option.value.at(0);
        else if(option.string_key == "remote-name")
            it++->remotename = true;
    }

    if(vm.contains("upload-file"))
    {
        for(auto it = requests.begin();
            auto& s : vm.at("upload-file").as<std::vector<std::string>>())
        {
            if(it == requests.end())
                break;

            it++->input = s;
        }
    }

    std::reverse(requests.begin(), requests.end());
    auto request_opt_gen = [requests]() mutable -> boost::optional<request_opt>
    {
        for(;;)
        {
            if(requests.empty())
                return boost::none;

            auto& request_info = requests.back();
            if(auto o = request_info.url_gen())
            {
                return request_opt(
                    o->result,
                    o->interpolate(request_info.output),
                    request_info.input,
                    request_info.remotename);
            }
            requests.pop_back();
        }
    };

    // --data options
    boost::optional<std::string> data;
    {
        auto append_striped = [&](std::istream& input)
        {
            char ch;
            while(input.get(ch))
            {
                if(ch != '\r' && ch != '\n' && ch != '\0')
                    data->push_back(ch);
            }
        };

        auto append_encoded = [&](core::string_view sv)
        {
            urls::encoding_opts opt;
            opt.space_as_plus = true;
            urls::encode(
                sv, urls::pchars, opt, urls::string_token::append_to(*data));
        };

        auto init = [&, init = false]() mutable
        {
            if(std::exchange(init, true))
                data->push_back('&');
            else
                data.emplace();
        };

        if(vm.contains("data"))
        {
            for(core::string_view sv :
                vm.at("data").as<std::vector<std::string>>())
            {
                init();

                if(sv.starts_with('@'))
                    append_striped(any_istream{ sv.substr(1) });
                else
                    data->append(sv);
            }
        }

        if(vm.contains("data-ascii"))
        {
            for(core::string_view sv :
                vm.at("data-ascii").as<std::vector<std::string>>())
            {
                init();

                if(sv.starts_with('@'))
                    append_striped(any_istream{ sv.substr(1) });
                else
                    data->append(sv);
            }
        }

        if(vm.contains("data-binary"))
        {
            for(core::string_view sv :
                vm.at("data-binary").as<std::vector<std::string>>())
            {
                init();

                if(sv.starts_with('@'))
                    any_istream{ sv.substr(1) }.append_to(*data);
                else
                    data->append(sv);
            }
        }

        if(vm.contains("data-raw"))
        {
            for(core::string_view sv :
                vm.at("data-raw").as<std::vector<std::string>>())
            {
                init();

                data->append(sv);
            }
        }

        if(vm.contains("data-urlencode"))
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
                        data->append(name);
                        data->push_back('=');
                    }
                    append_encoded(content);
                }
                else if(pos = sv.find('@'); pos != sv.npos)
                {
                    auto name     = sv.substr(0, pos);
                    auto filename = sv.substr(pos + 1);
                    if(!name.empty())
                    {
                        data->append(name);
                        data->push_back('=');
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

        if(data)
        {
            if(vm.contains("get"))
            {
                auto parse_rs = urls::parse_query(data.value());
                if(parse_rs.has_error())
                    throw std::runtime_error{ "Invalid data encoding" };
                oc.query = parse_rs->buffer();
            }
            else
            {
                oc.msg =
                    string_body{ *data, "application/x-www-form-urlencoded" };
            }
        }
    }

    // clang-format off
    if(int{ data.has_value() } + vm.contains("form") + vm.contains("json") + vm.contains("upload-file") > 1)
        throw std::runtime_error{ "You can only select one HTTP request method" };
    // clang-format on

    if(vm.contains("form") || vm.contains("form-string"))
    {
        auto form = multipart_form{};
        if(vm.contains("form"))
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
                        filename = fs::path{ value }.filename();

                    if(value == "-")
                    {
                        value.clear();
                        any_istream{ core::string_view{ "-" } }.append_to(
                            value);
                        is_file = false;
                    }
                    else if(!type)
                    {
                        type = ::mime_type(value);
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
        if(vm.contains("form-string"))
        {
            for(core::string_view sv :
                vm.at("form-string").as<std::vector<std::string>>())
            {
                if(auto pos = sv.find('='); pos != sv.npos)
                    form.append(false, sv.substr(0, pos), sv.substr(pos + 1));
                else
                    throw std::runtime_error("Illegally formatted input field");
            }
        }
        oc.msg = std::move(form);
    }

    if(vm.contains("json"))
    {
        std::string body;
        for(core::string_view sv : vm.at("json").as<std::vector<std::string>>())
        {
            if(sv.starts_with('@'))
                any_istream{ sv.substr(1) }.append_to(body);
            else
                body.append(sv);
        }
        oc.msg = string_body{ std::move(body), "application/json" };
    }

    if(vm.contains("url-query"))
    {
        std::string query;

        auto append_encoded = [&](core::string_view sv)
        {
            urls::encoding_opts opt;
            opt.space_as_plus = true;
            urls::encode(
                sv, urls::pchars, opt, urls::string_token::append_to(query));
        };

        for(core::string_view sv :
            vm.at("url-query").as<std::vector<std::string>>())
        {
            if(!query.empty())
                query.push_back('&');

            if(sv.starts_with("+"))
            {
                // as-is unencoded
                query.append(sv.substr(1));
            }
            else if(auto pos = sv.find('='); pos != sv.npos)
            {
                auto name    = sv.substr(0, pos);
                auto content = sv.substr(pos + 1);
                if(!name.empty())
                {
                    query.append(name);
                    query.push_back('=');
                }
                append_encoded(content);
            }
            else if(pos = sv.find('@'); pos != sv.npos)
            {
                auto name     = sv.substr(0, pos);
                auto filename = sv.substr(pos + 1);
                if(!name.empty())
                {
                    query.append(name);
                    query.push_back('=');
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

        if(!query.empty())
        {
            if(!oc.query.empty())
                oc.query.push_back('&');
            oc.query.append(query);
        }
    }

    if(vm.contains("connect-to"))
    {
        static constexpr auto parser = grammar::tuple_rule(
            urls::authority_rule,
            grammar::squelch(grammar::optional_rule(grammar::delim_rule(':'))),
            urls::authority_rule);

        std::vector<std::pair<urls::url, urls::url>> items;

        for(core::string_view sv :
            vm.at("connect-to").as<std::vector<std::string>>())
        {
            auto rs = grammar::parse(sv, parser);
            if(rs.has_error())
                throw std::runtime_error{ "bad --connect-to option" };
            items.emplace_back(
                get<0>(rs.value()).buffer(), get<1>(rs.value()).buffer());
        }

        oc.connect_to = [items = std::move(items)](urls::url& url)
        {
            for(const auto& [left, right] : items)
            {
                auto left_host = left.encoded_host();
                auto left_port = left.port();

                if(url.encoded_host() != left_host && !left_host.empty())
                    continue;

                if(effective_port(url) != left_port && !left_port.empty())
                    continue;

                if(!right.encoded_host().empty())
                    url.set_encoded_host(right.encoded_host());

                if(!right.port().empty())
                    url.set_port(right.port());

                break;
            }
        };
    }

    if(vm.count("resolve"))
    {
        static constexpr auto parser = grammar::tuple_rule(
            grammar::token_rule(grammar::all_chars - grammar::lut_chars(':')),
            grammar::squelch(grammar::delim_rule(':')),
            grammar::token_rule(grammar::digit_chars),
            grammar::squelch(grammar::delim_rule(':')),
            grammar::squelch(grammar::optional_rule(grammar::delim_rule('['))),
            grammar::variant_rule(
                urls::ipv4_address_rule, urls::ipv6_address_rule),
            grammar::squelch(grammar::optional_rule(grammar::delim_rule(']'))));

        std::vector<std::pair<urls::url, urls::url>> items;

        for(core::string_view sv :
            vm.at("resolve").as<std::vector<std::string>>())
        {
            auto rs = grammar::parse(sv, parser);
            if(rs.has_error())
                throw std::runtime_error{ "bad --resolve option" };
            auto [host, port, ip_address] = rs.value();
            auto& item                    = items.emplace_back();

            item.first.set_host(host);
            item.first.set_port(port);
            if(ip_address.index() == 0)
                item.second.set_host_ipv4(get<0>(ip_address));
            else
                item.second.set_host_ipv6(get<1>(ip_address));
        }

        oc.resolve_to = [items = std::move(items)](urls::url& url)
        {
            for(const auto& [left, right] : items)
            {
                if(url.encoded_host() != left.encoded_host() &&
                   left.encoded_host() != "*")
                    continue;

                if(effective_port(url) != left.port())
                    continue;

                url.set_encoded_host(right.encoded_host_address());
                break;
            }
        };
    }

    if(vm.contains("header"))
    {
        auto add_header = [&](core::string_view sv)
        {
            if(auto pos = sv.find(':'); pos != std::string::npos)
            {
                auto name  = sv.substr(0, pos);
                auto value = sv.substr(pos + 1);
                if(!value.empty())
                    oc.headers.set(name, value);
                else
                    oc.omitheaders.emplace_back(name);
            }
            else if(pos = sv.find(';'); pos != std::string::npos)
            {
                oc.headers.set(sv.substr(0, pos), "");
            }
        };

        for(core::string_view sv :
            vm.at("header").as<std::vector<std::string>>())
        {
            if(sv.starts_with('@'))
            {
                auto is   = any_istream{ sv.substr(1) };
                auto line = std::string{};
                while(std::getline(static_cast<std::istream&>(is), line))
                    add_header(line);
            }
            else
            {
                add_header(sv);
            }
        }
    }

    return { std::move(oc), std::move(ssl_ctx), std::move(request_opt_gen) };
}
