//
// Copyright (c) 2024 Mohammad Nejati
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/cppalliance/beast2
//

#include "progress_meter.hpp"

#include <numeric>

progress_meter::progress_meter(boost::optional<std::uint64_t> total)
    : total_{ total }
{
}

void
progress_meter::update(std::uint64_t bytes) noexcept
{
    auto now     = ch::steady_clock::now();
    auto elapsed = std::min(
        ch::duration_cast<ch::milliseconds>(now - last_update_),
        ch::milliseconds{ 1250 });
    auto shift = static_cast<std::size_t>((elapsed / 250).count());

    if(shift != 0)
    {
        last_update_ = now;

        std::move(window_.begin() + shift, window_.end(), window_.begin());
        std::fill(window_.end() - shift, window_.end(), 0);
    }

    transfered_    += bytes;
    window_.back() += bytes;
}

std::uint64_t
progress_meter::cur_rate() const
{
    auto now = ch::steady_clock::now();

    if(now - init_ < ch::seconds{ 1 })
        now = last_update_ + ch::milliseconds{ 250 };

    auto elapsed = std::min(
        ch::duration_cast<ch::milliseconds>(now - last_update_),
        ch::milliseconds{ 1250 });
    auto offset = static_cast<std::size_t>((elapsed / 250).count());

    return std::accumulate(
        window_.begin() + offset,
        offset ? window_.end() : std::prev(window_.end()),
        std::uint64_t{ 0 });
}

std::uint64_t
progress_meter::avg_rate() const
{
    auto elapsed =
        ch::duration_cast<ch::seconds>(ch::steady_clock::now() - init_).count();

    if(elapsed == 0)
        return 0;

    return transfered_ / static_cast<std::uint64_t>(elapsed);
}

boost::optional<std::uint64_t>
progress_meter::total() const
{
    return total_;
}

std::uint64_t
progress_meter::transfered() const
{
    return transfered_;
}

boost::optional<std::uint64_t>
progress_meter::remaining() const
{
    if(total_ && total_.value() >= transfered_)
        return total_.value() - transfered_;

    return boost::none;
}

boost::optional<int>
progress_meter::pct() const
{
    if(total_)
        return static_cast<int>(transfered_ * 100 / total_.value());

    return boost::none;
}

boost::optional<ch::hh_mm_ss<ch::seconds>>
progress_meter::eta() const
{
    auto re = remaining();
    if(!re)
        return boost::none;

    auto rate = cur_rate();

    if(rate == 0)
        rate = avg_rate();

    if(rate == 0)
        return boost::none;

    return ch::hh_mm_ss<ch::seconds>(ch::seconds{ re.value() / rate });
}
