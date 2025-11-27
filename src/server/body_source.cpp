//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include <boost/beast2/server/body_source.hpp>

namespace boost {
namespace beast2 {

body_source::
~body_source()
{
    if(! impl_)
        return;
    impl_->~impl();
    ::operator delete(impl_);
}

body_source&
body_source::
operator=(body_source&& other) noexcept
{
    if(&other == this)
        return *this;
    if(impl_)
    {
        impl_->~impl();
        ::operator delete(impl_);
    }
    impl_ = other.impl_;
    other.impl_ = nullptr;
    return *this;
}

} // beast2
} // boost
