//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

//
// Example usage of the logger with format string support.
//
// This file demonstrates how to use the logger with both the modern
// format string API (when std::format or fmtlib is available) and
// the legacy variadic template API.
//

#include "logger.hpp"
#include <string>

void example_usage()
{
    section log;

#if defined(BOOST_HTTP_IO_HAS_STD_FORMAT) || defined(BOOST_HTTP_IO_HAS_FMT)
    // Modern format string API (recommended when available)
    // Uses {}-style placeholders like Python's str.format() or Rust's format!()
    
    std::string client_ip = "192.168.1.100";
    std::string path = "/index.html";
    int status_code = 200;
    size_t bytes_sent = 4096;
    
    // Basic usage
    LOG_INF(log, "Server starting on port {}", 8080);
    
    // Multiple arguments
    LOG_INF(log, "Request from {} for {}", client_ip, path);
    
    // Mixed types
    LOG_INF(log, "Response: {} - {} bytes sent", status_code, bytes_sent);
    
    // Formatting options (when using fmtlib or std::format)
    double response_time = 123.456789;
    LOG_DBG(log, "Response time: {:.2f}ms", response_time);
    
#else
    // Legacy variadic template API (fallback)
    // Arguments are concatenated using operator<<
    
    std::string client_ip = "192.168.1.100";
    std::string path = "/index.html";
    int status_code = 200;
    size_t bytes_sent = 4096;
    
    // Basic usage
    LOG_INF(log, "Server starting on port ", 8080);
    
    // Multiple arguments
    LOG_INF(log, "Request from ", client_ip, " for ", path);
    
    // Mixed types
    LOG_INF(log, "Response: ", status_code, " - ", bytes_sent, " bytes sent");
    
    // Note: Formatting options are not available with stringstream API
    double response_time = 123.456789;
    LOG_DBG(log, "Response time: ", response_time, "ms");
    
#endif
}
