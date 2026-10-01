#pragma once

#include <easylocal/runners/first_improvement.hpp>

#include <chrono>
#include <cstddef>
#include <thread>
#include <type_traits>
#include <utility>

namespace easylocal::mwe::assignment::demo
{

struct SlowFirstImprovementParameters
{
    std::size_t max_evaluations{2000};
    std::size_t delay_ms{5};
};

namespace detail
{

template<class Evaluation>
class DelayedEvaluation
{
public:
    using solution_type = typename Evaluation::solution_type;
    using cost_type = typename Evaluation::cost_type;
    using evaluation_type = typename Evaluation::evaluation_type;
    using move_type = typename Evaluation::move_type;
    using candidate_type = typename Evaluation::candidate_type;

    DelayedEvaluation(
        Evaluation evaluation,
        const std::chrono::milliseconds delay)
        : evaluation_{std::move(evaluation)}, delay_{delay}
    {
    }

    [[nodiscard]]
    auto evaluate(const solution_type& solution) const -> evaluation_type
    {
        pause();
        return evaluation_.evaluate(solution);
    }

    [[nodiscard]]
    auto evaluate_move(
        const solution_type& solution,
        const evaluation_type& current,
        const move_type& move) const -> candidate_type
    {
        pause();
        return evaluation_.evaluate_move(solution, current, move);
    }

    void commit(
        solution_type& solution,
        evaluation_type& current,
        candidate_type&& candidate) const
    {
        evaluation_.commit(solution, current, std::move(candidate));
    }

private:
    void pause() const
    {
        if (delay_.count() != 0)
        {
            std::this_thread::sleep_for(delay_);
        }
    }

    Evaluation evaluation_;
    std::chrono::milliseconds delay_;
};

template<class Context>
class DelayedContext
{
public:
    using solution_type = typename Context::solution_type;
    using cost_type = typename Context::cost_type;
    using neighborhood_explorer_type = typename Context::neighborhood_explorer_type;

    DelayedContext(
        const Context& context,
        const std::chrono::milliseconds delay) noexcept
        : context_{context}, delay_{delay}
    {
    }

    [[nodiscard]]
    auto neighborhood_explorer() const noexcept
        -> const neighborhood_explorer_type&
    {
        return context_.neighborhood_explorer();
    }

    [[nodiscard]]
    auto evaluation() const
    {
        using evaluation_type =
            std::remove_cvref_t<decltype(context_.evaluation())>;
        return DelayedEvaluation<evaluation_type>{context_.evaluation(), delay_};
    }

    [[nodiscard]]
    auto better(const cost_type& candidate, const cost_type& reference) const
        -> bool
    {
        return context_.better(candidate, reference);
    }

private:
    const Context& context_;
    std::chrono::milliseconds delay_;
};

} // namespace detail

class SlowFirstImprovement
{
public:
    using parameters_type = SlowFirstImprovementParameters;

    explicit SlowFirstImprovement(SlowFirstImprovementParameters parameters)
        : parameters_{parameters}
    {
    }

    template<class Run>
    [[nodiscard]]
    auto run(Run& run, typename Run::solution_type solution) const
    {
        const auto context = delayed(run.context());
        auto delayed_run = run.with_context(context);
        return algorithm().run(delayed_run, std::move(solution));
    }

private:
    [[nodiscard]]
    auto algorithm() const -> easylocal::runners::FirstImprovement
    {
        return easylocal::runners::FirstImprovement{
            easylocal::runners::FirstImprovementParameters{
                .max_evaluations = parameters_.max_evaluations,
            }};
    }

    template<class Context>
    [[nodiscard]]
    auto delayed(const Context& context) const
    {
        return detail::DelayedContext<Context>{
            context,
            std::chrono::milliseconds{
                static_cast<std::chrono::milliseconds::rep>(
                    parameters_.delay_ms)},
        };
    }

    SlowFirstImprovementParameters parameters_;
};

} // namespace easylocal::mwe::assignment::demo
