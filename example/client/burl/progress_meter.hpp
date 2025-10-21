//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#ifndef BURL_PROGRESS_METER_HPP
#define BURL_PROGRESS_METER_HPP

#include <boost/core/detail/string_view.hpp>
#include <boost/optional/optional.hpp>

#include <array>
#include <chrono>
#include <cstdint>

namespace ch = std::chrono;

class progress_meter
{
    boost::optional<std::uint64_t> total_;
    std::uint64_t transfered_                 = {};
    std::array<std::uint64_t, 5> window_      = {};
    ch::steady_clock::time_point init_        = ch::steady_clock::now();
    ch::steady_clock::time_point last_update_ = ch::steady_clock::now();

public:
    progress_meter(boost::optional<std::uint64_t> total);

    void
    update(std::uint64_t bytes) noexcept;

    std::uint64_t
    cur_rate() const;

    std::uint64_t
    avg_rate() const;

    boost::optional<std::uint64_t>
    total() const;

    std::uint64_t
    transfered() const;

    boost::optional<std::uint64_t>
    remaining() const;

    boost::optional<int>
    pct() const;

    boost::optional<ch::hh_mm_ss<ch::seconds>>
    eta() const;
};

#endif
