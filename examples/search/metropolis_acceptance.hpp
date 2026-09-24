#pragma once

#include <cassert>
#include <cmath>
#include <concepts>
#include <functional>
#include <random>
#include <utility>

namespace easylocal::mwe::search
{

// Provisional MWE-local policy used to pressure-test SimulatedAnnealing.
// Its public shape and numerical semantics are intentionally not framework API.
template<class Energy, class Equivalent = std::equal_to<>>
class MetropolisAcceptance
{
public:
    explicit MetropolisAcceptance(
        Energy energy,
        Equivalent equivalent = {})
        : energy_{std::move(energy)},
          equivalent_{std::move(equivalent)}
    {
    }

    template<class Cost, std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    auto accept(
        const Cost& candidate,
        const Cost& current,
        const double temperature,
        RNG& rng) const -> bool
    {
        assert(temperature > 0.0);

        const auto candidate_energy =
            static_cast<double>(std::invoke(energy_, candidate));
        const auto current_energy =
            static_cast<double>(std::invoke(energy_, current));

        assert(std::isfinite(candidate_energy));
        assert(std::isfinite(current_energy));

        if (std::invoke(equivalent_, candidate_energy, current_energy))
        {
            return true;
        }

        if (candidate_energy < current_energy)
        {
            return true;
        }

        const auto probability =
            std::exp(-(candidate_energy - current_energy) / temperature);
        std::uniform_real_distribution<double> draw{0.0, 1.0};
        return draw(rng) < probability;
    }

private:
    Energy energy_;
    Equivalent equivalent_;
};

template<class Energy>
MetropolisAcceptance(Energy)
    -> MetropolisAcceptance<Energy>;

template<class Energy, class Equivalent>
MetropolisAcceptance(Energy, Equivalent)
    -> MetropolisAcceptance<Energy, Equivalent>;

} // namespace easylocal::mwe::search
