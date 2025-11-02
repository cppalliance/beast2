//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_POLYSTORE_HPP
#define BOOST_BEAST2_POLYSTORE_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/beast2/detail/call_traits.hpp>
#include <boost/beast2/detail/get_key_type.hpp>
#include <boost/beast2/detail/except.hpp>
#include <boost/beast2/detail/type_info.hpp>
#include <memory>
#include <type_traits>
#include <typeinfo>
#include <unordered_map>

namespace boost {
namespace beast2 {

/** A container of type-erased objects

    Objects are stored and retrieved by their type.
    Each type may be stored at most once. Types
    may specify a nested `key_type` to be used
    as the unique identifier instead of the type itself.
    In this case, the type must derive from the key type.

    @par Example
    @code
    struct A
    {
        int i = 1;
    };
    struct B
    {
        char c = '2';
    };
    struct C
    {
        double d;
    };
    struct D : C
    {
        using key_type = C;
        D()
        {
            d = 3.14;
        }
    };
    polystore ps;
    A& a = ps.emplace<A>();
    B& b = ps.insert(B{});
    C& c = ps.emplace<C>();
    assert(ps.get<A>().i == 1);
    assert(ps.get<B>().c == '2');
    assert(ps.get<C>().d == 3.14);
    ps.invoke([](A& a){ a.i = 0; });
    ps.invoke([](A const&, B& b){ b.c = 0; });
    assert(ps.get<A>().i == 0);
    assert(ps.get<B>().c == 0);
    @endcode
*/
class polystore
{
public:
    /** Destructor

        Objects stored in the container are destroyed.
    */
    BOOST_BEAST2_DECL
    ~polystore();

    /** Constructor
    */
    BOOST_BEAST2_DECL
    polystore();

    /** Return a pointer to a stored T, or nullptr
    */
    template<class T>
    T* find() noexcept;

    /** Return a reference to a stored T
        @throws std::bad_typeid if the type is not found.
    */
    template<class T>
    T& get();

    /** Construct and store an object of type T
        @param args Arguments forwarded to the constructor of T.
        @throws std::invalid_argument if an object of the same
            type (or key type) is already stored.
    */
    template<class T, class... Args>
    T& emplace(Args&&... args);

    /** Insert an object by moving or copying it into the container
        @param t The object to insert.
        @throws std::invalid_argument if an object of the same
            type (or key type) is already stored.
    */
    template<class T>
    T& insert(T&& t);

    /** Invoke a callable, injecting stored objects as arguments
        The callable is invoked with zero or more arguments.
        For each argument type, if an object of that type
        (or key type) is stored in the container, a reference
        to that object is passed to the callable.
        @par Example
        @code
        struct A { int i = 1; };
        polystore ps;
        ps.emplace<A>();
        ps.invoke([](A& a){ assert(a.i == 1; });
        @endcode
        @param f The callable to invoke.
        @return The result of the invocation.
        @throws std::bad_typeid if any argument type
            is not found in the container.
    */
    template<class F>
    auto
    invoke(F&& f) -> typename detail::call_traits<
        typename std::decay<F>::type>::return_type
    {
        return invoke(std::forward<F>(f), typename
            detail::call_traits< typename
                std::decay<F>::type>::arg_types{});
    }

private:
    template<class T> struct arg;
    template<class T> struct arg<T const&> : arg<T&> {};
    template<class T> struct arg<T const*> : arg<T*> {};
    template<class T> struct arg<T&>
    {
        T& operator()(polystore& ps) const
        {
            return ps.get<T>();
        }
    };
    template<class T> struct arg<T*>
    {
        T* operator()(polystore& ps) const
        {
            return ps.find<T>();
        }
    };

    template<class F, class... Args>
    auto
    invoke(F&& f, detail::type_list<Args...> const&) ->
        typename detail::call_traits<typename
            std::decay<F>::type>::return_type
    {
        return std::forward<F>(f)(arg<Args>()(*this)...);
    }

    struct BOOST_SYMBOL_VISIBLE any
    {
        BOOST_BEAST2_DECL virtual ~any();
        virtual void* get() noexcept = 0;
    };

    struct hash
    {
        std::size_t operator()(
            detail::type_info const* p) const noexcept
        {
            return p->hash_code();
        }
    };

    struct eq
    {
        bool operator()(
            detail::type_info const* p0,
            detail::type_info const* p1) const noexcept
        {
            return *p0 == *p1;
        }
    };

    std::unordered_map<
        detail::type_info const*,
        std::unique_ptr<any>, hash, eq> m_;
};

//------------------------------------------------

template<class T>
T* polystore::find() noexcept
{
    auto const it = m_.find(&detail::get_type_info<T>());
    if(it == m_.end())
        return nullptr;
    return static_cast<T*>(it->second->get());
}

template<class T>
T& polystore::get()
{
    auto t = find<T>();
    if(! t)
        detail::throw_bad_typeid();
    return *t;
}

template<class T, class... Args>
T& polystore::emplace(Args&&... args)
{
    struct impl : any
    {
        T t;
        explicit impl(Args&&... args)
            : t(std::forward<Args>(args)...)
        {
        }
        void* get() noexcept override
        {
            return &t;
        }
    };
    using U = detail::get_key_type<T>;
    static_assert(std::is_base_of<U, T>::value, "");
    auto result = m_.emplace(&detail::get_type_info<U>(),
        std::unique_ptr<any>(new impl(
            std::forward<Args>(args)...)));
    if (!result.second)
        detail::throw_invalid_argument();
    return *static_cast<T*>(result.first->second->get());
}

template<class T>
T& polystore::insert(T&& t)
{
    return emplace<typename
        std::remove_cv<T>::type>(
            std::forward<T>(t));
}

} // beast2
} // boost

#endif
