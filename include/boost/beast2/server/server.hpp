//
// Copyright (c) 2022 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_SERVER_SERVER_HPP
#define BOOST_BEAST2_SERVER_SERVER_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/rts/context.hpp>
#include <memory>
#include <type_traits>

namespace boost {
namespace beast2 {

class log_sections;

/** A generic server interface

    In this model a server contains zero or more parts, where each part
    undergoes two-phase initialization. First the part is constructed by
    calling @ref make_part. The @ref run function invokes `run()` on each
    part. When @ref stop is called, each part has its `stop()` member
    invoked. And when the server object is destroyed, all the parts are
    destroyed in reverse order of construction.

    The server also contains an `rts::services` collection VFALCO TODO
*/
class BOOST_SYMBOL_VISIBLE
    server
{
public:
    /** Base class for all server parts
    */
    class BOOST_SYMBOL_VISIBLE
        part
    {
    public:
        BOOST_BEAST2_DECL
        virtual ~part();

        /** Called once when the server starts
        */
        virtual void start() = 0;

        /** Called once when the server stops

            This function is invoked from an
            implementation-defined foreign thread.
        */
        virtual void stop() = 0;
    };

    server(server const&) = delete;
    server& operator=(server const&) = delete;

    /** Destructor
    */
    BOOST_BEAST2_DECL
    ~server();

    /** Constructor
    */
    BOOST_BEAST2_DECL
    server();

    BOOST_BEAST2_DECL
    bool is_stopping() const noexcept;

    BOOST_BEAST2_DECL
    rts::context& services() noexcept;

    BOOST_BEAST2_DECL
    log_sections& sections() noexcept;

    BOOST_BEAST2_DECL
    void install(std::unique_ptr<part> pp);

    /** Stop the server
    */
    virtual void stop() = 0;

protected:
    /** Call start on each part

        @par Thread Safety
        May not be called concurrently
    */
    BOOST_BEAST2_DECL void do_start();

    BOOST_BEAST2_DECL void do_stop();

private:
    enum state : char;
    struct impl;
    impl* impl_;
};

//------------------------------------------------

/** Construct a new server part
*/
template<
    class Part,
    class Server,
    class... Args>
auto
emplace_part(
    Server& srv,
    Args&&... args) -> Part&
        // VFALCO I couldn't get this to work and I'm also not sure I want it to
        //typename std::enable_if<detail::derived_from<server::part, Server>::value, Part&>::type
{
    static_assert(
        std::is_convertible<Part*, server::part*>::value,
        "Type requirements not met.");

    auto p = std::unique_ptr<Part>(new Part(
        srv, std::forward<Args>(args)...));
    auto& part = *p;
    srv.install(std::move(p));
    return part;
}

} // beast2
} // boost

#endif
