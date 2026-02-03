| | [Docs](https://develop.beast2.cpp.al/) | [GitHub Actions](https://github.com/) | [Drone](https://drone.io/) | [Codecov](https://codecov.io) |
|:--|:--|:--|:--|:--|
| [`master`](https://github.com/cppalliance/beast2/tree/master) | [![Documentation](https://img.shields.io/badge/docs-master-brightgreen.svg)](https://master.beast2.cpp.al/) | [![CI](https://github.com/cppalliance/beast2/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/cppalliance/beast2/actions/workflows/ci.yml?query=branch%3Amaster) | [![Build Status](https://drone.cpp.al/api/badges/cppalliance/beast2/status.svg?ref=refs/heads/master)](https://drone.cpp.al/cppalliance/beast2/branches) | [![codecov](https://codecov.io/gh/cppalliance/beast2/branch/master/graph/badge.svg)](https://app.codecov.io/gh/cppalliance/beast2/tree/master) |
| [`develop`](https://github.com/cppalliance/beast2/tree/develop) | [![Documentation](https://img.shields.io/badge/docs-develop-brightgreen.svg)](https://develop.beast2.cpp.al/) | [![CI](https://github.com/cppalliance/beast2/actions/workflows/ci.yml/badge.svg?branch=develop)](https://github.com/cppalliance/beast2/actions/workflows/ci.yml?query=branch%3Adevelop) | [![Build Status](https://drone.cpp.al/api/badges/cppalliance/beast2/status.svg?ref=refs/heads/develop)](https://drone.cpp.al/cppalliance/beast2/branches) | [![codecov](https://codecov.io/gh/cppalliance/beast2/branch/develop/graph/badge.svg)](https://app.codecov.io/gh/cppalliance/beast2/tree/develop) |

# Beast2

Coroutines-Only Networking for Modern C++

## The Problem

Boost.Asio transformed C++ networking. It also burdened every call site with templates, platform headers, and implementation machinery, yet its quarter century of field experience produced capable, working abstractions.

The C++26 `std::execution` API offers a different model, designed to support heterogenous computing. Our research indicates it optimizes for the wrong constraints: TCP servers don't run on GPUs. Networking demands zero-allocation steady-state, type erasure without indirection, and ABI stability across (e.g.) SSL implementations. C++26 delivers things that networking doesn't need, and none of the things that networking does need.

## The Design Principle

Peter Dimov insists we begin with what developers actually write:

```cpp
auto [ec, n] = co_await sock.read_some( buf );
```

One line. No platform headers. No completion handlers. Just coroutines. This simplicity drives every architectural decision:

- **Clean call sites** — no platform or implementation headers leak through
- **Extensible concepts** — user-defined buffer sequences and executors; stream wrappers without templates
- **Zero-alloc steady state** — no coroutine frame allocations in hot paths
- **Minimal template exposure** — implementation details stay in the translation unit
- **Allocation-free type erasure** — user types preserved without small buffers or malloc
- **Fine-grained execution control** — strands, processor affinity, priority scheduling

Boost.Asio gave us the field experience. These design choices reflect what we learned.

## The Library Family

Beast2 is an umbrella term for a family of libraries that work together:

**Boost.Capy** — The execution foundation, optimized for async I/O. Core types: `task<T>`, `thread_pool`, `strand`, `coro_lock`. Tasks propagate executor context through `await_suspend` automatically, guaranteeing correct executor affinity. Provides allocation-free type erasure, frame allocators for coroutine memory, automatic stop token propagation, and buffer algorithms. **This should go into the C++ Standard Library**.

**Boost.Corosio** — Coroutine-only portable networking and I/O that wrap per-platform implementations. Every operation returns an awaitable; no callbacks. Core types: `socket`, `acceptor`, `resolver`, `strand`, `io_context`, SSL stream wrappers (WolfSSL and OpenSSL), non-templated streams, scatter/gather buffers, native `std::stop_token` cancellation.

**Boost.Http** — Sans-I/O HTTP/1.1 with strict RFC compliance. Protocol logic isolated from I/O for testability and framework independence. Core types: `request`, `response`, `parser`, `serializer`, `router`. Non-templated containers, memory-bounded parsers, body streaming, Express.js-style routing, automatic gzip/deflate/brotli application, and an extensive collection of HTTP "middleware" algorithms such as multipart/form processing, cookie management, bcrypt, and more.

**Boost.Websocket** — WebSocket algorithms and structures. Same philosophy.

**Boost.Beast2** — High-level HTTP and WebSocket servers. Express.js-style routing, multithreaded, idiomatic C++.

**Boost.Burl** — High-level HTTP client. The features of curl, the ergonomics of coroutines, and the design sensibility of Python Requests.

Each of these libraries is built for direct use. Boost.Capy can be standardized without risky socket or SSL decisions. Combine as needed.

## Technical Advantages

Beast2 continues the Boost tradition of innovation through working code.

### Zero-Allocation Steady-State

`capy::task` coroutine frames reuse memory (delete before dispatch). Type erasure is achieved without small-buffer optimizations or malloc. User types are preserved through the abstraction boundary. Key insight:

> _The coroutine frame allocation we cannot avoid, pays for all the type-erasure we need._

### Templates Where They Matter

Boost.Asio:

```cpp
template< class AsyncStream, class CompletionToken >
auto do_io( AsyncStream&, CompletionToken&& );
```

Boost.Corosio:

```cpp
auto do_io( capy::any_stream& ); // returns awaitable
```

No loss of generality. Buffer sequences and executors remain concept-driven. Stream wrappers require no templates. The drawbacks of `std::execution` can be seen in a single line:

```cpp
connect( sndr, rcvr ); // C++26, unavoidable templates
```

Here, the compiler must see all the types (or perform expensive type-erasure, which is inconvenient at this call site and uncompetitive). No encapsulation. Long compile times. ABI fragility. Unfriendly to network programs.

### ABI Stability by Design

Boost.Asio:

```cpp
template< class AsyncStream >
class ssl_stream
{
    AsyncStream stream_;
    ...
```

Boost.Corosio:

```cpp
class wolfssl_stream : public tls_stream
{
    io_stream& wrapped_stream_;
    ...
```

Stream algorithms see `tls_stream&` not `wolfssl_stream&` and can be written as ordinary functions (non-templates). Link your HTTP library against Corosio and WolfSSL. Relink against OpenSSL. No recompilation. SSL implementation becomes a runtime decision with zero ABI consequence. Have both in the same executable if you want (maybe useful for researchers).

### No Configuration Macros

There is only one configuration of the library:

Boost.Asio:

```cpp
#define BOOST_ASIO_NO_SCHEDULER_MUTEX 1

{
    asio::io_context ioc;
}
```

Boost.Corosio:

```cpp
{
    corosio::unsafe_io_context uioc; // no macro needed

    corosio::io_context ioc; // use one or both, linker removes whats not used
}
```

One library. One object file. Runtime configuration.

### Implementation Hiding

No platform headers at call sites. No implementation structures in user code. Translation-unit isolation by default. ABI is stable by design, and thus if adopted in the standard can be evolved rather than freezing.

## Boost.Beast2

Unlike its predecessor, Beast2 is _full-featured_ and **high-level**. Everything is in scope:

```cpp
app.use( "/", serve_static( "C:\\Users\\Vinnie\\my-website" ) );
```

Express.js patterns. Multithreaded execution. C++ performance.

## Summary

The Beast2 family of libraries responds to the pain points related by users and delivers everything they want. It demonstrates through working code what modern C++ networking requires: coroutine-native design, fast compilation without loss of generality, ABI stability across implementations, and good performance, while preserving every capability that made Boost.Asio great.

This is the future of C++ networking.

---

_Boost.Beast2 is under active development. API subject to change._
