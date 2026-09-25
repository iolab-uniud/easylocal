#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <random>
#include <type_traits>

namespace easylocal::search
{

template<class Cost>
concept numeric_cost =
    (std::integral<std::remove_cv_t<Cost>> ||
     std::floating_point<std::remove_cv_t<Cost>>) &&
    (!std::same_as<std::remove_cv_t<Cost>, bool>);

class MetropolisAcceptance
{
public:
    template<numeric_cost Cost, std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    auto accept(
        const Cost candidate,
        const Cost current,
        const double temperature,
        RNG& rng) const -> bool
    {
        assert(std::isfinite(temperature));
        assert(temperature > 0.0);

        const auto candidate_energy = static_cast<long double>(candidate);
        const auto current_energy = static_cast<long double>(current);

        assert(std::isfinite(candidate_energy));
        assert(std::isfinite(current_energy));

        const auto delta = candidate_energy - current_energy;
        if (delta <= 0.0L)
        {
            return true;
        }

        const auto probability = std::exp(
            -delta / static_cast<long double>(temperature));
        std::uniform_real_distribution<double> draw{0.0, 1.0};
        return draw(rng) < static_cast<double>(probability);
    }
};

} // namespace easylocal::search
