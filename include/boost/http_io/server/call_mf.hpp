//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_SERVER_CALL_MF_HPP
#define BOOST_HTTP_IO_SERVER_CALL_MF_HPP

#include <boost/http_io/detail/config.hpp>
#include <type_traits>
#include <utility>

namespace boost {
namespace http_io {

namespace detail {
// Primary template
template<class>
struct result_of;

// Partial specialization for callable types
template<class F, class... Args>
struct result_of<F(Args...)>
{
private:
    template<class G, class... A>
    static auto test(int) -> decltype(std::declval<G>()(std::declval<A>()...));

    template<class, class...>
    static void test(...);

public:
    using type = decltype(test<F, Args...>(0));
};
template<class T>
using result_of_t = typename result_of<T>::type;
} // detail

template<class T, class MemFn>
class call_mf_impl
{
    T* obj_;
    MemFn memfn_;

public:
    call_mf_impl(T* obj, MemFn memfn)
        : obj_(obj), memfn_(memfn)
    {
    }

    template<class... Args>
    auto operator()(Args&&... args) const
        -> detail::result_of_t<MemFn(T*, Args&&...)>
    {
        return (obj_->*memfn_)(std::forward<Args>(args)...);
    }
};

template<class MemFn, class T>
inline call_mf_impl<T, MemFn> call_mf(MemFn memfn, T* obj)
{
    return call_mf_impl<T, MemFn>(obj, memfn);
}

} // http_io
} // boost

#endif
