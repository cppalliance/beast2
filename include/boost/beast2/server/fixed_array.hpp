//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_FIXED_ARRAY_HPP
#define BOOST_BEAST2_SERVER_FIXED_ARRAY_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/core/span.hpp>
#include <boost/assert.hpp>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <new>
#include <stdexcept>
#include <utility>

template<class T>
class fixed_array;

/** A type-erased fixed_array
*/
class any_fixed_array
{
public:
    /** Destructor

        All elements are destroyed in reverse order.
    */
    ~any_fixed_array()
    {
        if(t_)
            destroy_(this);
    }

    /** Constructor
    */
    any_fixed_array() = default;

    /** Constructor

        Ownership of all elements in the moved-from object is transferred.
    */
    any_fixed_array(
        any_fixed_array&& other) noexcept
        : any_fixed_array()
    {
        swap(*this, other);
    }

    /** Constructor

        Ownership of all elements in the moved-from object is transferred.

        @par Postconditions
        @code
        other.size() == 0 && other.capacity() == 0
        @endcode
    */
    template<class T>
    any_fixed_array(
        fixed_array<T>&& other) noexcept
        : t_(other.t_)
        , n_(other.n_)
        , cap_(other.cap_)
        , destroy_(&destroy<T>)
    {
        other.t_ = nullptr;
        other.n_ = 0;
        other.cap_ = 0;
    }

    /** Constructor

        Ownership of all elements in the moved-from object is transferred.

        @par Postconditions
        @code
        other.size() == 0 && other.capacity() == 0
        @endcode
    */
    any_fixed_array&
    operator=(
        any_fixed_array&& other) noexcept
    {
        any_fixed_array temp;
        swap(*this, temp);
        swap(other, *this);
        return *this;
    }

    /** Return the number of elements
    */
    std::size_t size() const noexcept
    {
        return n_;
    }

    /** Return a typed span of elements

        This function returns the span of elements contained in the array.
        The specified type `T` must match the type of element used
        to construct the array, or else the behavior is undefined.

        @tparam T The type of element used to construct the array.
    */
    template<class T>
    boost::span<T>
    to_span() noexcept
    {
        return { reinterpret_cast<T*>(t_), n_ };
    }

    /** Swap objects
    */
    friend void swap(
        any_fixed_array& a0,
        any_fixed_array& a1) noexcept
    {
        std::swap(a0.t_, a1.t_);
        std::swap(a0.n_, a1.n_);
        std::swap(a0.cap_, a1.cap_);
        std::swap(a0.destroy_, a1.destroy_);
    }

private:
    template<class T>
    friend class fixed_array;

    template<class T>
    static void destroy(any_fixed_array* p)
    {
        fixed_array<T> v(*p);
    }

    void* t_ = nullptr;
    std::size_t n_ = 0;
    std::size_t cap_ = 0;
    void(*destroy_)(any_fixed_array*) = 0;
};

//------------------------------------------------

/** An append-only array with a fixed capacity

    This container is used to hold elements which
    are all constructed at once, where the elements
    do not require move constructability or assignability.
*/
template<class T>
class fixed_array
{
public:
    using value_type = T;
    using reference = T&;
    using pointer = T*;
    using iterator = T*;
    using const_reference = T const&;
    using const_pointer = T const*;
    using const_iterator = T const*;
    using difference_type = std::ptrdiff_t;
    using size_type = std::size_t;

    ~fixed_array()
    {
        if(! t_)
            return;
        while(n_--)
            t_[n_].~T();
        std::allocator<T>{}.deallocate(t_, cap_);
    }

    /** Constructor

        The moved-from object will have zero
        capacity and zero size.
    */
    fixed_array(fixed_array&& other) noexcept
    {
        t_ = other.t_;
        n_ = other.n_;
        cap_ = other.cap_;
        other.t_ = nullptr;
        other.n_ = 0;
        other.cap_ = 0;
    }

    /** Constructor

        The array will have the specified capacity.

        @par Postconditions
        ```
        size() == 0 &&  capacity() == cap
        ```
    */
    explicit
    fixed_array(std::size_t cap)
        : t_(std::allocator<T>{}.allocate(cap))
        , n_(0)
        , cap_(cap)
    {
    }

    std::size_t
    capacity() const noexcept
    {
        return cap_;
    }

    std::size_t
    size() const noexcept
    {
        return n_;
    }

    bool
    is_full() const noexcept
    {
        return n_ >= cap_;
    }

    /** Return a pointer to the beginning of the array
    */
    T* data() noexcept
    {
        return t_;
    }

    /** Return a pointer to the beginning of the array
    */
    T const* data() const noexcept
    {
        return t_;
    }   

    reference operator[](std::size_t i)
    {
        BOOST_ASSERT(i < n_);
        return t_[i];
    }

    const_reference operator[](std::size_t i) const
    {
        BOOST_ASSERT(i < n_);
        return t_[i];
    }

    reference at(std::size_t i)
    {
        if(i < n_)
            return t_[i];
        // VFALCO use detail::throw_out_of_range when it is available
        throw std::out_of_range("i >= size()");
    }

    const_reference at(std::size_t i) const
    {
        if(i < n_)
            return t_[i];
        // VFALCO use detail::throw_out_of_range when it is available
        throw std::out_of_range("i >= size()");
    }

    template<class... Args>
    T& emplace_back(Args&&... args)
    {
        if(is_full())
            // VFALCO use detail::throw_out_of_range when it is available
            throw std::out_of_range("full");
        auto p = t_ + n_;
        ::new(p) T(std::forward<Args>(args)...);
        ++n_;
        return *p;
    }

    const_iterator
    begin() const noexcept
    {
        return t_;
    }

    const_iterator
    end() const noexcept
    {
        return t_ + n_;
    }

    iterator
    begin() noexcept
    {
        return t_;
    }

    iterator
    end() noexcept
    {
        return t_ + n_;
    }

private:
#if 0
//#if __cplusplus < 201703L // gcc nonconforming
    static_assert(
        alignof(T) <=
            alignof(std::max_align_t),
        "T must not be overaligned");
//#endif
#endif

    friend class any_fixed_array;

    fixed_array(
        any_fixed_array& v) noexcept
        : t_(reinterpret_cast<T*>(v.t_))
        , n_(v.n_)
        , cap_(v.cap_)
    {
    }

    T* t_ = nullptr;
    std::size_t n_;
    std::size_t cap_;
};

#endif
