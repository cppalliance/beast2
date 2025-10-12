//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/http_io
//

#ifndef BOOST_HTTP_IO_SERVER_ROUTER_HPP
#define BOOST_HTTP_IO_SERVER_ROUTER_HPP

#include <boost/http_io/detail/config.hpp>
#include <boost/http_proto/method.hpp>
#include <boost/core/detail/string_view.hpp>
#include <boost/assert.hpp>
#include <memory>
#include <new>
#include <regex>
#include <string>
#include <vector>

namespace boost {
namespace http_io {

//template<class Base> class router;

class router_base
{
public:
protected:

};

/** A container mapping HTTP requests to handlers
*/
template<class Base>
class router : public router_base
{
public:
    class handler;

    struct factory
    {
        virtual ~factory() = default;
        virtual void operator()(handler& dest) const = 0;
    };

    template<
        class Factory,
        class Base_,
        class... Args>
    friend void emplace(
        router<Base_>& r,
        http_proto::method method,
        core::string_view pattern,
        Args&&... args);

    /** Add a GET route
    */
    template<
        class Factory,
        class... Args>
    void get(
        core::string_view pattern,
        Args&&... args);

    bool find(
        handler& dest,
        http_proto::method method,
        core::string_view path)
    {
        (void)method;
        (void)path;
        (*f_)(dest);
        return true;
    }

private:
    struct entry
    {
        std::regex rx;
        std::unique_ptr<factory> f;
    };

    std::vector<entry> v_;
    std::unique_ptr<factory> f_;
};

//------------------------------------------------

template<class Base>
class router<Base>::handler
{
public:
    ~handler()
    {
        if(! p_)
            return;
        if( pb_)
            pb_->~Base();
        std::allocator<U>().deallocate(p_, cap_);
    }

    handler() = default;

    void reset()
    {
        if(! pb_)
            return;
        pb_->~Base();
        pb_ = nullptr;
    }

    Base* get()
    {
        BOOST_ASSERT(pb_ != nullptr);
        return pb_;
    }

    Base* operator->()
    {
        return get();
    }

    void reserve(std::size_t bytes)
    {
        BOOST_ASSERT(pb_ == nullptr);
        auto n = (bytes + sizeof(U) - 1) / sizeof(U);
        if(cap_ >= n)
            return;
        auto p = std::allocator<U>().allocate(n);
        if(p_)
            std::allocator<U>().deallocate(p_, cap_);
        p_ = p;
        cap_ = n;
    }

    template<class T, class... Args>
    void emplace(Args&&... args)
    {
        if(pb_)
        {
            pb_->~Base();
            pb_ = nullptr;
        }
        reserve(sizeof(T));
        pb_ = ::new(p_) T(std::forward<Args>(args)...);
    }

private:
    explicit handler(std::size_t bytes)
    {
        reserve(bytes);
    }

    using U = std::max_align_t;

    U* p_ = nullptr;
    Base* pb_ = nullptr;
    std::size_t cap_ = 0; // of U
};

//------------------------------------------------

template<
    class Factory,
    class Base,
    class... Args>
void
emplace(
    router<Base>& r,
    http_proto::method method,
    core::string_view path,
    Args&&... args)
{
    (void)method;
    (void)path;
    using router_type = router<Base>;
    std::unique_ptr<typename router_type::factory> p(
        new Factory(std::forward<Args>(args)...));
    r.f_ = std::move(p);
}

template<class Base>
template<
    class Factory,
    class... Args>
void
router<Base>::
get(
    core::string_view pattern,
    Args&&... args)
{
    emplace<Factory>(
        *this,
        http_proto::method::get,
        pattern,
        std::forward<Args>(args)...);
}

} // http_io
} // boost

#endif
