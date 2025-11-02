//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_APPLICATION_HPP
#define BOOST_BEAST2_APPLICATION_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/rts/context.hpp>
#include <memory>
#include <type_traits>

namespace boost {
namespace beast2 {

class log_sections;

/** A collection of type-erased parts and process state

    An object of this type holds a collection of type-erased parts,
    where each part undergoes two-phase initialization. First the part is
    constructed by calling @ref make_part. The @ref run function invokes
    `run()` on each part. When @ref stop is called, each part has its
    `stop()` member invoked. And when the application object is destroyed,
    all the parts are destroyed in reverse order of construction.

    The server also contains an `rts::services` collection VFALCO TODO
*/
class BOOST_SYMBOL_VISIBLE
    application
{
public:
    /** Base class for all parts
    */
    class BOOST_SYMBOL_VISIBLE
        part
    {
    public:
        BOOST_BEAST2_DECL
        virtual ~part();

        /** Called once when @ref application::start is called

            Parts are started in the order they were created.
        */
        virtual void start() = 0;

        /** Called once when @ref application::stop is called

            Parts are stopped in the reverse order they were created.
            This function is invoked from an
            implementation-defined foreign thread.
        */
        virtual void stop() = 0;
    };

    application(application const&) = delete;
    application& operator=(application const&) = delete;

    /** Destructor

        All parts will be destroyed in the reverse order of creation.
    */
    BOOST_BEAST2_DECL
    ~application();

    /** Constructor
    */
    BOOST_BEAST2_DECL
    application();

    /** Invoke @ref part::start on each part in creation order
        Each call is performed synchronously; this function blocks until each
        part returns. Only one invocation of `start` is permitted.
    */
    BOOST_BEAST2_DECL
    void start();

    /** Invoke @ref part::stop on each part in reverse creation order
        This function is idempotent and returns immediately.
        @par Thread Safety
        May be called concurrently.
    */
    BOOST_BEAST2_DECL
    void stop();

    /** Wait until the application is stopped
    */
    BOOST_BEAST2_DECL
    void join();

    /** Install a new application part
    */
    template<class Part, class... Args>
    auto emplace(Args&&... args) -> Part&
        // VFALCO I couldn't get this to work and I'm also not sure I want it to
        //typename std::enable_if<detail::derived_from<applicati0on::part, Server>::value, Part&>::type
    {
        static_assert(
            std::is_convertible<Part*, application::part*>::value,
            "Type requirements not met.");

        auto p = std::unique_ptr<Part>(new Part(
            *this, std::forward<Args>(args)...));
        auto& part = *p;
        insert(std::move(p));
        return part;
    }

    BOOST_BEAST2_DECL rts::context& services() noexcept;
    BOOST_BEAST2_DECL log_sections& sections() noexcept;

private:
    BOOST_BEAST2_DECL
    void insert(std::unique_ptr<part> pp);

private:
    enum state : char;
    struct impl;
    impl* impl_;
};

} // beast2
} // boost

#endif
