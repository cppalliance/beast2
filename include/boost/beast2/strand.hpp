//
// Copyright (c) 2025 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BOOST_BEAST2_STRAND_HPP
#define BOOST_BEAST2_STRAND_HPP

#include <boost/beast2/detail/config.hpp>
#include <boost/asio/strand.hpp>

namespace boost {
namespace beast2 {

/** A strand which can be constructed implicitly from an executor
*/
template<class Executor>
class strand
{
public:
    /// The type of the underlying executor.
    using inner_executor_type = Executor;

    strand()
        : st()
    {
    }

    /// Construct a strand for the specified executor.
    template <typename Executor1>
    explicit strand(const Executor1& e,
    constraint_t<
    conditional_t<
    !is_same<Executor1, strand>::value,
    is_convertible<Executor1, Executor>,
    false_type
    >::value
    > = 0)
    : executor_(e),
    impl_(strand::create_implementation(executor_))
    {
    }

    /// Copy constructor.
    strand(const strand& other) noexcept
    : executor_(other.executor_),
    impl_(other.impl_)
    {
    }

    /// Converting constructor.
    /**
    * This constructor is only valid if the @c OtherExecutor type is convertible
    * to @c Executor.
    */
    template <class OtherExecutor>
    strand(
    const strand<OtherExecutor>& other) noexcept
    : executor_(other.executor_),
    impl_(other.impl_)
    {
    }

    /// Assignment operator.
    strand& operator=(const strand& other) noexcept
    {
    executor_ = other.executor_;
    impl_ = other.impl_;
    return *this;
    }

    /// Converting assignment operator.
    /**
    * This assignment operator is only valid if the @c OtherExecutor type is
    * convertible to @c Executor.
    */
    template <class OtherExecutor>
    strand& operator=(
    const strand<OtherExecutor>& other) noexcept
    {
    executor_ = other.executor_;
    impl_ = other.impl_;
    return *this;
    }

    /// Move constructor.
    strand(strand&& other) noexcept
    : executor_(static_cast<Executor&&>(other.executor_)),
    impl_(static_cast<implementation_type&&>(other.impl_))
    {
    }

    /// Converting move constructor.
    /**
    * This constructor is only valid if the @c OtherExecutor type is convertible
    * to @c Executor.
    */
    template <class OtherExecutor>
    strand(strand<OtherExecutor>&& other) noexcept
    : executor_(static_cast<OtherExecutor&&>(other.executor_)),
    impl_(static_cast<implementation_type&&>(other.impl_))
    {
    }

    /// Move assignment operator.
    strand& operator=(strand&& other) noexcept
    {
    executor_ = static_cast<Executor&&>(other.executor_);
    impl_ = static_cast<implementation_type&&>(other.impl_);
    return *this;
    }

    /// Converting move assignment operator.
    /**
    * This assignment operator is only valid if the @c OtherExecutor type is
    * convertible to @c Executor.
    */
    template <class OtherExecutor>
    strand& operator=(strand<OtherExecutor>&& other) noexcept
    {
    executor_ = static_cast<OtherExecutor&&>(other.executor_);
    impl_ = static_cast<implementation_type&&>(other.impl_);
    return *this;
    }

    /// Destructor.
    ~strand() noexcept
    {
    }

    /// Obtain the underlying executor.
    inner_executor_type get_inner_executor() const noexcept
    {
    return executor_;
    }

    /// Forward a query to the underlying executor.
    /**
    * Do not call this function directly. It is intended for use with the
    * boost::asio::query customisation point.
    *
    * For example:
    * @code boost::asio::strand<my_executor_type> ex = ...;
    * if (boost::asio::query(ex, boost::asio::execution::blocking)
    *       == boost::asio::execution::blocking.never)
    *   ... @endcode
    */
    template <typename Property>
    constraint_t<
    can_query<const Executor&, Property>::value,
    conditional_t<
    is_convertible<Property, execution::blocking_t>::value,
    execution::blocking_t,
    query_result_t<const Executor&, Property>
    >
    > query(const Property& p) const
    noexcept(is_nothrow_query<const Executor&, Property>::value)
    {
    return this->query_helper(
    is_convertible<Property, execution::blocking_t>(), p);
    }

    /// Forward a requirement to the underlying executor.
    /**
    * Do not call this function directly. It is intended for use with the
    * boost::asio::require customisation point.
    *
    * For example:
    * @code boost::asio::strand<my_executor_type> ex1 = ...;
    * auto ex2 = boost::asio::require(ex1,
    *     boost::asio::execution::blocking.never); @endcode
    */
    template <typename Property>
    constraint_t<
    can_require<const Executor&, Property>::value
    && !is_convertible<Property, execution::blocking_t::always_t>::value,
    strand<decay_t<require_result_t<const Executor&, Property>>>
    > require(const Property& p) const
    noexcept(is_nothrow_require<const Executor&, Property>::value)
    {
    return strand<decay_t<require_result_t<const Executor&, Property>>>(
    boost::asio::require(executor_, p), impl_);
    }

    /// Forward a preference to the underlying executor.
    /**
    * Do not call this function directly. It is intended for use with the
    * boost::asio::prefer customisation point.
    *
    * For example:
    * @code boost::asio::strand<my_executor_type> ex1 = ...;
    * auto ex2 = boost::asio::prefer(ex1,
    *     boost::asio::execution::blocking.never); @endcode
    */
    template <typename Property>
    constraint_t<
    can_prefer<const Executor&, Property>::value
    && !is_convertible<Property, execution::blocking_t::always_t>::value,
    strand<decay_t<prefer_result_t<const Executor&, Property>>>
    > prefer(const Property& p) const
    noexcept(is_nothrow_prefer<const Executor&, Property>::value)
    {
    return strand<decay_t<prefer_result_t<const Executor&, Property>>>(
    boost::asio::prefer(executor_, p), impl_);
    }

    #if !defined(BOOST_ASIO_NO_TS_EXECUTORS)
    /// Obtain the underlying execution context.
    execution_context& context() const noexcept
    {
    return executor_.context();
    }

    /// Inform the strand that it has some outstanding work to do.
    /**
    * The strand delegates this call to its underlying executor.
    */
    void on_work_started() const noexcept
    {
    executor_.on_work_started();
    }

    /// Inform the strand that some work is no longer outstanding.
    /**
    * The strand delegates this call to its underlying executor.
    */
    void on_work_finished() const noexcept
    {
    executor_.on_work_finished();
    }
    #endif // !defined(BOOST_ASIO_NO_TS_EXECUTORS)

    /// Request the strand to invoke the given function object.
    /**
    * This function is used to ask the strand to execute the given function
    * object on its underlying executor. The function object will be executed
    * according to the properties of the underlying executor.
    *
    * @param f The function object to be called. The executor will make
    * a copy of the handler object as required. The function signature of the
    * function object must be: @code void function(); @endcode
    */
    template <typename Function>
    constraint_t<
    traits::execute_member<const Executor&, Function>::is_valid,
    void
    > execute(Function&& f) const
    {
    detail::strand_executor_service::execute(impl_,
    executor_, static_cast<Function&&>(f));
    }

    #if !defined(BOOST_ASIO_NO_TS_EXECUTORS)
    /// Request the strand to invoke the given function object.
    /**
    * This function is used to ask the strand to execute the given function
    * object on its underlying executor. The function object will be executed
    * inside this function if the strand is not otherwise busy and if the
    * underlying executor's @c dispatch() function is also able to execute the
    * function before returning.
    *
    * @param f The function object to be called. The executor will make
    * a copy of the handler object as required. The function signature of the
    * function object must be: @code void function(); @endcode
    *
    * @param a An allocator that may be used by the executor to allocate the
    * internal storage needed for function invocation.
    */
    template <typename Function, typename Allocator>
    void dispatch(Function&& f, const Allocator& a) const
    {
    detail::strand_executor_service::dispatch(impl_,
    executor_, static_cast<Function&&>(f), a);
    }

    /// Request the strand to invoke the given function object.
    /**
    * This function is used to ask the executor to execute the given function
    * object. The function object will never be executed inside this function.
    * Instead, it will be scheduled by the underlying executor's post function.
    *
    * @param f The function object to be called. The executor will make
    * a copy of the handler object as required. The function signature of the
    * function object must be: @code void function(); @endcode
    *
    * @param a An allocator that may be used by the executor to allocate the
    * internal storage needed for function invocation.
    */
    template <typename Function, typename Allocator>
    void post(Function&& f, const Allocator& a) const
    {
    detail::strand_executor_service::post(impl_,
    executor_, static_cast<Function&&>(f), a);
    }

    /// Request the strand to invoke the given function object.
    /**
    * This function is used to ask the executor to execute the given function
    * object. The function object will never be executed inside this function.
    * Instead, it will be scheduled by the underlying executor's defer function.
    *
    * @param f The function object to be called. The executor will make
    * a copy of the handler object as required. The function signature of the
    * function object must be: @code void function(); @endcode
    *
    * @param a An allocator that may be used by the executor to allocate the
    * internal storage needed for function invocation.
    */
    template <typename Function, typename Allocator>
    void defer(Function&& f, const Allocator& a) const
    {
    detail::strand_executor_service::defer(impl_,
    executor_, static_cast<Function&&>(f), a);
    }
    #endif // !defined(BOOST_ASIO_NO_TS_EXECUTORS)

    /// Determine whether the strand is running in the current thread.
    /**
    * @return @c true if the current thread is executing a function that was
    * submitted to the strand using post(), dispatch() or defer(). Otherwise
    * returns @c false.
    */
    bool running_in_this_thread() const noexcept
    {
    return detail::strand_executor_service::running_in_this_thread(impl_);
    }

    /// Compare two strands for equality.
    /**
    * Two strands are equal if they refer to the same ordered, non-concurrent
    * state.
    */
    friend bool operator==(const strand& a, const strand& b) noexcept
    {
    return a.impl_ == b.impl_;
    }

    /// Compare two strands for inequality.
    /**
    * Two strands are equal if they refer to the same ordered, non-concurrent
    * state.
    */
    friend bool operator!=(const strand& a, const strand& b) noexcept
    {
    return a.impl_ != b.impl_;
    }

    #if defined(GENERATING_DOCUMENTATION)
    private:
    #endif // defined(GENERATING_DOCUMENTATION)
    typedef detail::strand_executor_service::implementation_type
    implementation_type;

    template <typename InnerExecutor>
    static implementation_type create_implementation(const InnerExecutor& ex,
    constraint_t<
    can_query<InnerExecutor, execution::context_t>::value
    > = 0)
    {
    return use_service<detail::strand_executor_service>(
    boost::asio::query(ex, execution::context)).create_implementation();
    }

    template <typename InnerExecutor>
    static implementation_type create_implementation(const InnerExecutor& ex,
    constraint_t<
    !can_query<InnerExecutor, execution::context_t>::value
    > = 0)
    {
    return use_service<detail::strand_executor_service>(
    ex.context()).create_implementation();
    }

    strand(const Executor& ex, const implementation_type& impl)
    : executor_(ex),
    impl_(impl)
    {
    }

    template <typename Property>
    query_result_t<const Executor&, Property> query_helper(
    false_type, const Property& property) const
    {
    return boost::asio::query(executor_, property);
    }

    template <typename Property>
    execution::blocking_t query_helper(true_type, const Property& property) const
    {
    execution::blocking_t result = boost::asio::query(executor_, property);
    return result == execution::blocking.always
    ? execution::blocking.possibly : result;
    }

private:
    asio::strand<Executor> st_;
};

} // beast2
} // boost

#endif
