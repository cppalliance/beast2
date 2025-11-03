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
#include <boost/beast2/detail/except.hpp>
#include <boost/beast2/detail/type_info.hpp>
#include <boost/core/detail/static_assert.hpp>
#include <memory>
#include <type_traits>
#include <typeinfo>

namespace boost {
namespace beast2 {

/** A container of type-erased objects

    Objects are stored and retrieved by their type.
    Each type may be stored at most once. Types
    may specify a nested `key_type` to be used
    as the unique identifier instead of the type
    itself. In this case, a reference to the type
    must be convertible to a reference to the key type.

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
    invoke(ps, [](A& a){ a.i = 0; });
    invoke(ps, [](A const&, B& b){ b.c = 0; });
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
        @par Thread Safety
        May be called concurrently with other `const` member functions.
    */
    template<class T>
    T* find() const noexcept;

    /** Return a reference to a stored T
        @par Thread Safety
        May be called concurrently with other `const` member functions.
        @throws std::bad_typeid if the type is not found.
    */
    template<class T>
    T& get() const;

    /** Construct and store an object of type T
        Objects stored with this function will never
        be returned from @ref find or @ref get.
        @par Exception Safety
        Strong guarantee.
        @par Thread Safety
        Not thread-safe.
        @param args Arguments forwarded to the constructor of T.
    */
    template<class T, class... Args>
    T& emplace(Args&&... args);

    /** Insert an object by moving or copying it into the container
        Objects stored with this function will never
        be returned from @ref find or @ref get.
        @par Exception Safety
        Strong guarantee.
        @par Thread Safety
        Not thread-safe.
        @param t The object to insert.
    */
    template<class T>
    T& insert(T&& t)
    {
        return emplace<typename
            std::remove_cv<T>::type>(
                std::forward<T>(t));
    }

    /** Construct and store an object of type T
        @par Exception Safety
        Strong guarantee.
        @par Thread Safety
        Not thread-safe.
        @throws std::invalid_argument if an object of the same
            type (or key type) is already stored.
        @param args Arguments forwarded to the constructor of T.
    */
    template<class T, class... Args>
    T& emplace_unique(Args&&... args);

    /** Insert an object by moving or copying it into the container
        @par Exception Safety
        Strong guarantee.
        @par Thread Safety
        Not thread-safe.
        @throws std::invalid_argument if an object of the same
            type (or key type) is already stored.
        @param t The object to insert.
    */
    template<class T>
    T& insert_unique(T&& t)
    {
        return emplace_unique<typename
            std::remove_cv<T>::type>(
                std::forward<T>(t));
    }

protected:
    struct any;
    class elements;

    BOOST_BEAST2_DECL
    elements
    get_elements() noexcept;

private:
    template<class, class = void> struct has_start;
    template<class, class = void> struct has_stop;
    template<class T> struct any_impl;
    using ptr = std::unique_ptr<any>;
    struct impl;

    template<class T, class = void>
    struct get_key_type_impl
    {
        using type = T;
    };

    template<class T>
    struct get_key_type_impl<T,
        decltype(void(typename T::key_type()))>
    {
        using type = typename T::key_type;
    };

    // Alias for T::key_type if it exists, otherwise T
    template<class T>
    using get_key_type =
        typename get_key_type_impl<T>::type;

    BOOST_BEAST2_DECL any& get(std::size_t i);
    BOOST_BEAST2_DECL void* find(
        detail::type_info const& ti) const noexcept;
    BOOST_BEAST2_DECL void insert(
        detail::type_info const*, ptr);

    impl* impl_;
};

//------------------------------------------------

struct BOOST_SYMBOL_VISIBLE
    polystore::any
{
    BOOST_BEAST2_DECL virtual ~any();
    virtual void* get() noexcept = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
};

//------------------------------------------------

class polystore::elements
{
public:
    std::size_t size() const noexcept
    {
        return n_;
    }

    any& operator[](
        std::size_t i) noexcept
    {
        return ps_.get(i);
    }

private:
    friend class polystore;

    elements(
        std::size_t n,
        polystore& ps)
        : n_(n)
        , ps_(ps)
    {
    }

    std::size_t n_;
    polystore& ps_;
};

//------------------------------------------------

template<class T, class>
struct polystore::has_start : std::false_type {};

template<class T>
struct polystore::has_start<T, typename std::enable_if<
    std::is_same<decltype(std::declval<T>().start()),
        void>::value>::type> : std::true_type {};

template<class T, class>
struct polystore::has_stop : std::false_type {};

template<class T>
struct polystore::has_stop<T, typename std::enable_if<
    std::is_same<decltype(std::declval<T>().stop()),
        void>::value>::type> : std::true_type {};

template<class T>
struct polystore::any_impl : polystore::any
{
    T t;

    template<class... Args>
    explicit any_impl(Args&&... args)
        : t(std::forward<Args>(args)...)
    {
    }

    void* get() noexcept override
    {
        using U = get_key_type<T>;
        return &static_cast<U&>(t);
    }

    void start() override
    {
        do_start(has_start<T>{});
    }

    void stop() override
    {
        do_stop(has_stop<T>{});
    }

    void do_start(std::true_type)
    {
        t.start();
    }

    void do_start(std::false_type)
    {
    }

    void do_stop(std::true_type)
    {
        t.stop();
    }

    void do_stop(std::false_type)
    {
    }
};

//------------------------------------------------

template<class T>
T* polystore::find() const noexcept
{
    // can't find void
    static_assert(! std::is_same<T, void>::value, "");
    return static_cast<T*>(find(
        detail::get_type_info<T>()));
}

template<class T>
T& polystore::get() const
{
    auto t = find<T>();
    if(! t)
        detail::throw_bad_typeid();
    return *t;
}

template<class T, class... Args>
T& polystore::emplace(Args&&... args)
{
    using U = get_key_type<T>;
    BOOST_CORE_STATIC_ASSERT(
        std::is_convertible<T&, U&>::value);
    std::unique_ptr<any_impl<T>> p(new any_impl<T>(
        std::forward<Args>(args)...));
    auto& t = p->t;
    insert(nullptr, std::move(p));
    return t;
}

template<class T, class... Args>
T& polystore::emplace_unique(Args&&... args)
{
    using U = get_key_type<T>;
    BOOST_CORE_STATIC_ASSERT(
        std::is_convertible<T&, U&>::value);
    std::unique_ptr<any_impl<T>> p(new any_impl<T>(
        std::forward<Args>(args)...));
    auto& t = p->t;
    insert(&detail::get_type_info<U>(), std::move(p));
    return t;
}

//------------------------------------------------

namespace detail {

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
invoke(polystore& ps, F&& f,
    detail::type_list<Args...> const&) ->
        typename detail::call_traits<typename
            std::decay<F>::type>::return_type
{
    return std::forward<F>(f)(arg<Args>()(ps)...);
}

} // detail

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
    @throws std::bad_typeid if any reference argument
        types are not found in the container.
*/
template<class F>
auto
invoke(polystore& ps, F&& f) ->
    typename detail::call_traits<
        typename std::decay<F>::type>::return_type
{
    return detail::invoke(ps, std::forward<F>(f),
        typename detail::call_traits< typename
            std::decay<F>::type>::arg_types{});
}

} // beast2
} // boost

#endif
