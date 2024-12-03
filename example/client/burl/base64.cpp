//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#include "base64.hpp"

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
