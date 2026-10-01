#include <easylocal/core/aggregation.hpp>
#include <easylocal/helpers/cursor_moves.hpp>
#include "cost_components.hpp"
#include "cost_deltas.hpp"
#include "neighborhood_explorer.hpp"
#include "solution_manager.hpp"

#include <cstddef>
#include <iostream>
#include <string_view>

namespace exam = easylocal::mwe::exam_timetabling;

namespace
{

auto expect(const bool condition, const std::string_view description) -> bool
{
    if (!condition)
    {
        std::cerr << "FAILED: " << description << '\n';
        return false;
    }
    return true;
}

} // namespace

int main()
{
    bool ok = true;
    const exam::ExamTimetablingInstance instance{
        .exam_count = 4,
        .timeslot_count = 3,
        .conflicts = {
            {0, 1, 4},
            {0, 2, 2},
            {1, 3, 3},
            {2, 3, 5},
        },
    };
    const exam::ExamTimetable initial{
        .timeslot_by_exam = {0, 0, 1, 2},
    };

    const exam::ExamTimetablingSolutionManager manager{instance};
    const exam::MoveExamNeighborhoodExplorer neighborhood{manager};
    const exam::StudentConflictComponent conflicts{instance};
    const exam::ConsecutiveExamComponent consecutive{instance};
    const exam::TimeslotLoadComponent load{instance};
    const exam::StudentConflictComponent conflict_component{instance};
    const exam::ConsecutiveExamDeltaEvaluator consecutive_delta{instance};
    const exam::TimeslotLoadDeltaEvaluator load_delta{instance};

    const auto initial_conflicts = conflicts.evaluate(initial);
    const auto initial_consecutive = consecutive.evaluate(initial);
    const auto initial_load = load.evaluate(initial);
    const auto aggregate = easylocal::aggregation::weighted_sum{
        exam::penalty_type{1000},
        exam::penalty_type{10},
        exam::penalty_type{1},
    };
    const auto initial_cost = aggregate(
        initial_conflicts,
        initial_consecutive,
        initial_load);

    ok &= expect(initial_cost == 1000 * initial_conflicts.penalty +
                                10 * initial_consecutive.penalty +
                                initial_load.penalty,
        "exam timetabling uses a transparent three-component weighted sum");

    std::size_t move_count = 0;
    for (const auto move : easylocal::moves(neighborhood, initial))
    {
        ++move_count;
        auto candidate = initial;
        neighborhood.make_move(candidate, move);

        ok &= expect(
            initial_conflicts + conflict_component.delta_evaluate(initial, move) ==
                conflicts.evaluate(candidate),
            "student-conflict delta matches full evaluation");
        ok &= expect(
            initial_consecutive + consecutive_delta.delta_evaluate(initial, move) ==
                consecutive.evaluate(candidate),
            "consecutive-exam delta matches full evaluation");
        ok &= expect(
            initial_load + load_delta.delta_evaluate(initial, move) ==
                load.evaluate(candidate),
            "timeslot-load delta matches full evaluation");
    }

    ok &= expect(move_count == initial.timeslot_by_exam.size() * 2,
        "MoveExam enumerates every alternative timeslot exactly once");

    return ok ? 0 : 1;
}
