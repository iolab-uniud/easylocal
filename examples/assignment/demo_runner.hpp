#pragma once

// SlowFirstImprovement, a runner for the demos: First Improvement that waits
// before each evaluation, so that a run can be watched and stopped.

#include <easylocal/runners/first_improvement.hpp>
#include <easylocal/utils/limit.hpp>

#include <chrono>
#include <cstddef>
#include <thread>
#include <utility>

namespace assignment::demo
{

// The parameters of SlowFirstImprovement: its evaluation budget and the wait
// before each evaluation, in milliseconds.
struct SlowFirstImprovementParameters
{
    easylocal::limit max_evaluations{2000};
    std::size_t delay_ms{5};
};

namespace detail
{

template<class Evaluation>
class DelayedEvaluation
{
public:
    using solution_type = Evaluation::solution_type;
    using cost_type = Evaluation::cost_type;
    using evaluation_type = Evaluation::evaluation_type;
    using move_type = Evaluation::move_type;
    using candidate_type = Evaluation::candidate_type;

    DelayedEvaluation(Evaluation evaluation, std::chrono::milliseconds delay)
        : evaluation_{std::move(evaluation)}, delay_{delay}
    {
    }

    evaluation_type evaluate(const solution_type& solution) const
    {
        pause();
        return evaluation_.evaluate(solution);
    }

    candidate_type evaluate_move(
        const solution_type& solution,
        const evaluation_type& current,
        const move_type& move) const
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
            std::this_thread::sleep_for(delay_);
    }

    Evaluation evaluation_;
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

    // First Improvement on the same run, with every evaluation delayed: the
    // target, the budget and the time limit of the run still hold.
    template<class Run>
    auto run(Run& run, Run::solution_type solution) const
    {
        auto delayed_run = run.with_evaluation([this](auto evaluation) {
            return detail::DelayedEvaluation<decltype(evaluation)>{
                std::move(evaluation),
                delay()};
        });
        return algorithm().run(delayed_run, std::move(solution));
    }

private:
    easylocal::runners::FirstImprovement algorithm() const
    {
        return easylocal::runners::FirstImprovement{
            easylocal::runners::FirstImprovementParameters{
                .max_evaluations = parameters_.max_evaluations,
            }};
    }

    std::chrono::milliseconds delay() const
    {
        return std::chrono::milliseconds{
            static_cast<std::chrono::milliseconds::rep>(parameters_.delay_ms)};
    }

    SlowFirstImprovementParameters parameters_;
};

} // namespace assignment::demo
