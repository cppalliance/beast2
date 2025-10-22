//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BURL_OPTIONS_HPP
#define BURL_OPTIONS_HPP

#include "message.hpp"

#include <boost/asio/ssl/context.hpp>
#include <boost/http_proto/fields.hpp>
#include <boost/optional/optional.hpp>
#include <boost/url/url.hpp>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <set>
#include <vector>

namespace asio       = boost::asio;
namespace ch         = std::chrono;
namespace fs         = std::filesystem;
namespace http_proto = boost::http_proto;
namespace urls       = boost::urls;

struct operation_config
{
    using duration_type = ch::steady_clock::duration;

    duration_type timeout          = duration_type::max();
    duration_type expect100timeout = ch::seconds{ 1 };
    duration_type connect_timeout  = duration_type::max();
    boost::optional<duration_type> retry_delay;
    boost::optional<duration_type> retry_maxtime;
    bool disallow_username_in_url = false;
    boost::optional<std::size_t> recvpersecond;
    boost::optional<std::size_t> sendpersecond;
    bool encoding              = false;
    bool create_dirs           = false;
    std::uint64_t maxredirs    = 50;
    std::uint64_t max_filesize = std::numeric_limits<std::uint64_t>::max();
    bool tcp_nodelay           = true;
    std::uint64_t req_retry    = 0;
    std::uint16_t parallel_max = 1;
    bool retry_connrefused     = false;
    bool retry_all_errors      = false;
    bool nokeepalive           = false;
    bool post301               = false;
    bool post302               = false;
    bool post303               = false;
    std::set<urls::scheme> proto_redir{ urls::scheme::http,
                                        urls::scheme::https };
    fs::path unix_socket_path;
    std::function<void(urls::url&)> connect_to;
    std::function<void(urls::url&)> resolve_to;
    bool http10 = false;
    bool http11 = true;
    bool ipv4   = false;
    bool ipv6   = false;
    boost::optional<std::string> useragent;
    boost::optional<std::string> userpwd;
    bool enable_cookies = false;
    std::vector<std::string> cookies;
    std::vector<fs::path> cookiefiles;
    fs::path cookiejar;
    boost::optional<std::uint64_t> resume_from;
    bool resume_from_current = false;
    fs::path headerfile;
    urls::url referer;
    bool autoreferer  = false;
    bool failonerror  = false;
    bool failearly    = false;
    bool failwithbody = false;
    bool rm_partial   = false;
    bool use_httpget  = false;
    boost::optional<std::string> request_target;
    http_proto::fields headers;
    std::vector<std::string> omitheaders;
    bool show_headers        = false;
    bool cookiesession       = false;
    bool no_body             = false;
    bool content_disposition = false;
    bool unrestricted_auth   = false;
    bool followlocation      = false;
    bool nobuffer            = false;
    bool globoff             = false;
    bool noprogress          = false;
    bool skip_existing       = false;
    bool terminal_binary_ok  = false;
    fs::path output_dir;
    boost::optional<std::string> range;
    urls::url proxy;
    boost::optional<std::string> customrequest;
    std::string query;
    message msg;
};

struct request_opt
{
    std::string url;
    fs::path output;
    fs::path input;
    bool remotename = false;
};

struct parse_args_result
{
    operation_config oc;
    asio::ssl::context ssl_ctx;
    std::function<boost::optional<request_opt>()> ropt_gen;
};

parse_args_result
parse_args(int argc, char* argv[]);

#endif
