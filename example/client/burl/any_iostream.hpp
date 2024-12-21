//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BURL_ANY_IOSTREAM_HPP
#define BURL_ANY_IOSTREAM_HPP

#include <boost/core/detail/string_view.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <variant>

namespace fs   = std::filesystem;
namespace core = boost::core;

class any_ostream
{
    std::variant<std::ofstream, std::ostream*> stream_;
    bool is_tty_ = false;
    fs::path path_;

public:
    any_ostream();

    any_ostream(core::string_view path, bool append = false);

    any_ostream(fs::path path, bool append = false);

    bool
    is_tty() const noexcept;

    bool
    remove_file();

    operator std::ostream&();

    template<typename T>
    std::ostream&
    operator<<(const T& data)
    {
        static_cast<std::ostream&>(*this) << data;
        return *this;
    }
};

class any_istream
{
    std::variant<std::ifstream, std::istream*> stream_;

public:
    any_istream(core::string_view path);

    any_istream(fs::path path);

    void
    append_to(std::string& s);

    operator std::istream&();

    template<typename T>
    std::istream&
    operator>>(T& data)
    {
        static_cast<std::istream&>(*this) >> data;
        return *this;
    }
};

#endif
