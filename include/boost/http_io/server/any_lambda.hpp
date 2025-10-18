//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_SERVER_ANY_LAMBDA_HPP
#define BOOST_HTTP_IO_SERVER_ANY_LAMBDA_HPP

#include <boost/http_io/detail/config.hpp>

#include <cstddef>
#include <cstring>
#include <functional>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace boost {
namespace http_io {

namespace detail {

struct A { void f(); int g() const; virtual void h(); };
struct B { virtual void f(); };
struct C : virtual B { void g(); };

constexpr std::size_t max_size(std::size_t a, std::size_t b) noexcept
{
    return a > b ? a : b;
}

constexpr std::size_t max_sizeof_mfp() noexcept
{
    return max_size(
        max_size(
            max_size(sizeof(void (A::*)()), sizeof(int (A::*)() const)),
            sizeof(void (B::*)())
        ),
        sizeof(void (C::*)())
    );
}

} // detail

template<class Signature>
struct any_lambda; // primary template left undefined

template<class R, class... Args>
struct any_lambda<R(Args...)>
{
private:
    // Adjust as desired — must fit your expected lambdas
    static constexpr std::size_t BufferSize = detail::max_sizeof_mfp();
    static constexpr std::size_t BufferAlign = alignof(std::max_align_t);

    using InvokeFn = R(*)(void*, Args&&...);
    using CopyFn   = void(*)(void*, const void*);
    using MoveFn   = void(*)(void*, void*);
    using DestroyFn= void(*)(void*);

    typename std::aligned_storage<BufferSize, BufferAlign>::type buffer;
    InvokeFn invoke = nullptr;
    CopyFn copy = nullptr;
    MoveFn move = nullptr;
    DestroyFn destroy = nullptr;

public:
    any_lambda() = default;

    any_lambda(std::nullptr_t) noexcept {}

    any_lambda(const any_lambda& other)
    {
        if (other.invoke)
        {
            invoke = other.invoke;
            copy = other.copy;
            move = other.move;
            destroy = other.destroy;
            copy(&buffer, &other.buffer);
        }
    }

    any_lambda(any_lambda&& other) noexcept
    {
        if (other.invoke)
        {
            invoke = other.invoke;
            copy = other.copy;
            move = other.move;
            destroy = other.destroy;
            move(&buffer, &other.buffer);
            other.reset();
        }
    }

    template<class F>
    any_lambda(F f)
    {
        static_assert(!std::is_same<any_lambda, typename std::decay<F>::type>::value,
                      "cannot construct from self");
        static_assert(sizeof(F) <= BufferSize, "lambda too large for buffer");
        static_assert(alignof(F) <= BufferAlign, "lambda alignment too strict");

        new (&buffer) F(std::move(f));

        invoke = [](void* p, Args&&... args) -> R {
            return (*static_cast<F*>(p))(std::forward<Args>(args)...);
        };
        copy = [](void* dst, const void* src) {
            new (dst) F(*static_cast<const F*>(src));
        };
        move = [](void* dst, void* src) {
            new (dst) F(std::move(*static_cast<F*>(src)));
            static_cast<F*>(src)->~F();
        };
        destroy = [](void* p) { static_cast<F*>(p)->~F(); };
    }

    any_lambda& operator=(const any_lambda& other)
    {
        if (this != &other)
        {
            reset();
            if (other.invoke)
            {
                invoke = other.invoke;
                copy = other.copy;
                move = other.move;
                destroy = other.destroy;
                copy(&buffer, &other.buffer);
            }
        }
        return *this;
    }

    any_lambda& operator=(any_lambda&& other) noexcept
    {
        if (this != &other)
        {
            reset();
            if (other.invoke)
            {
                invoke = other.invoke;
                copy = other.copy;
                move = other.move;
                destroy = other.destroy;
                move(&buffer, &other.buffer);
                other.reset();
            }
        }
        return *this;
    }

    ~any_lambda() { reset(); }

    void reset()
    {
        if (destroy)
            destroy(&buffer);
        invoke = nullptr;//copy = move = destroy = nullptr;
    }

    explicit operator bool() const noexcept { return invoke != nullptr; }

    R operator()(Args... args) const
    {
        if (!invoke)
            throw std::bad_function_call();
        return invoke(const_cast<void*>(static_cast<const void*>(&buffer)),
                      std::forward<Args>(args)...);
    }
};
} // http_io
} // boost

#endif
