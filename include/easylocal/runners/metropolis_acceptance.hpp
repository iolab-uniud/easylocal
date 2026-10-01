#pragma once

#include <easylocal/core/cost.hpp>

#include <cassert>
#include <cmath>
#include <concepts>
#include <limits>
#include <random>
#include <type_traits>

namespace easylocal::runners
{

template<class Cost>
concept numeric_cost =
    (std::integral<std::remove_cv_t<Cost>> ||
     std::floating_point<std::remove_cv_t<Cost>>) &&
    (!std::same_as<std::remove_cv_t<Cost>, bool>);

namespace detail
{

template<class Cost>
concept metropolis_cost = delta_cost<Cost>;

} // namespace detail

class MetropolisAcceptance
{
public:
    template<detail::metropolis_cost Cost, std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    auto accept(
        const Cost& candidate,
        const Cost& current,
        const double temperature,
        RNG& rng) const -> bool
    {
        assert(std::isfinite(temperature));
        assert(temperature > 0.0);

        const auto delta = static_cast<long double>(candidate - current);
        assert(!std::isnan(delta));

        if (delta <= 0.0L)
        {
            return true;
        }
        if (std::isinf(delta))
        {
            return false;
        }

        const auto probability = std::exp(
            -delta / static_cast<long double>(temperature));
        std::uniform_real_distribution<double> draw{0.0, 1.0};
        return draw(rng) < static_cast<double>(probability);
    }
};

} // namespace easylocal::runners
