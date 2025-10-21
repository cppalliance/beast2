//
// Copyright (c) 2025 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include "error.hpp"

namespace
{
const boost::system::error_category&
error_category()
{
    static const struct : boost::system::error_category
    {
        const char*
        name() const noexcept override
        {
            return "burl";
        }

        std::string
        message(int ev) const override
        {
            switch(static_cast<error>(ev))
            {
            case error::binary_output_to_tty:
                return "binary output to tty";
            default:
                return "Unknown burl error";
            }
        }
    } category;

    return category;
};
} // namespace

std::error_code
make_error_code(error e)
{
    return { static_cast<int>(e), error_category() };
}
