#pragma once

/// \file
/// solvers::two_stage(): for hierarchical costs, a pipeline of a first stage on
/// the hard cost until it reaches zero and a second one on the whole cost.

#include <easylocal/solvers/pipeline.hpp>

#include <random>
#include <utility>

namespace easylocal::solvers
{

/// A pipeline of two stages for hierarchical costs: `first` on the hard cost
/// until it reaches zero, then `second` on the whole cost from its solution.
///
/// It is `(stage("first", first) & until_feasible()) | stage("second",
/// second)`, so its parameters are `first.*` and `second.*`, and the first
/// stage's attempts are set through them or with `stage<0>()`. Requires runners
/// with the same Input and Solution, the first with a hierarchical cost
/// (`cost::hierarchical`).
template<
    std::uniform_random_bit_generator RNG = std::mt19937_64,
    class FirstRunner,
    class SecondRunner>
[[nodiscard]]
auto two_stage(FirstRunner first, SecondRunner second)
{
    return solvers::pipeline<RNG>(
        stage("first", std::move(first)).until_feasible(),
        stage("second", std::move(second)));
}

/// A pipeline of two stages for hierarchical costs with the same runner: on the
/// hard cost until it reaches zero, then on the whole cost.
///
/// It is `two_stage(runner, runner)`. Requires a runner with a hierarchical
/// cost (`cost::hierarchical`).
template<std::uniform_random_bit_generator RNG = std::mt19937_64, class Runner>
[[nodiscard]]
auto two_stage(Runner runner)
{
    auto second = runner;
    return two_stage<RNG>(std::move(runner), std::move(second));
}

} // namespace easylocal::solvers
