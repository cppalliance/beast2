//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include "any_iostream.hpp"

#ifdef _MSC_VER
#include <io.h>
#else
#include <unistd.h>
#endif

namespace
{
bool
is_tty(FILE* stream)
{
#ifdef _MSC_VER
    return _isatty(_fileno(stream));
#else
    return isatty(fileno(stream));
#endif
}
} // namespace

any_ostream::any_ostream(core::string_view path, bool append)
    : any_ostream{ fs::path{ path.begin(), path.end() }, append }
{
}

any_ostream::any_ostream(fs::path path, bool append)
{
    if(path == "-")
    {
        stream_.emplace<std::ostream*>(&std::cout);
        is_tty_ = ::is_tty(stdout);
    }
    else if(path == "%")
    {
        stream_.emplace<std::ostream*>(&std::cerr);
        is_tty_ = ::is_tty(stderr);
    }
    else
    {
        auto& f = stream_.emplace<std::ofstream>();
        f.exceptions(std::ofstream::badbit);
        if(append)
            f.open(path, std::ofstream::app);
        else
            f.open(path);
        if(!f.is_open())
            throw std::runtime_error{ "Couldn't open file" };
    }
}

bool
any_ostream::is_tty() const noexcept
{
    return is_tty_;
}

void
any_ostream::close()
{
    if(auto* s = std::get_if<std::ofstream>(&stream_))
        s->close();
}

any_ostream::
operator std::ostream&()
{
    if(auto* s = std::get_if<std::ofstream>(&stream_))
        return *s;
    return *std::get<std::ostream*>(stream_);
}

// -----------------------------------------------------------------------------

any_istream::any_istream(core::string_view path)
    : any_istream{ fs::path{ path.begin(), path.end() } }
{
}

any_istream::any_istream(fs::path path)
{
    if(path == "-")
    {
        stream_.emplace<std::istream*>(&std::cin);
    }
    else
    {
        auto& f = stream_.emplace<std::ifstream>();
        f.exceptions(std::ifstream::badbit);
        f.open(path);
        if(!f.is_open())
            throw std::runtime_error{ "Couldn't open file" };
    }
}

void
any_istream::append_to(std::string& s)
{
    s.append(
        std::istreambuf_iterator<char>{ static_cast<std::istream&>(*this) },
        {});
}

any_istream::
operator std::istream&()
{
    if(auto* s = std::get_if<std::ifstream>(&stream_))
        return *s;
    return *std::get<std::istream*>(stream_);
}
