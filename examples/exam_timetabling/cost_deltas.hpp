#pragma once

#include "cost_components.hpp"
#include "move.hpp"

#include <cassert>
#include <vector>

namespace easylocal::mwe::exam_timetabling
{

class ConsecutiveExamDeltaEvaluator
{
public:
    explicit ConsecutiveExamDeltaEvaluator(
        const ExamTimetablingInstance& instance) noexcept
        : instance_{instance}
    {
    }

    [[nodiscard]]
    auto delta_evaluate(
        const ExamTimetable& solution,
        const MoveExam& move) const noexcept -> penalty_type
    {
        assert(move.exam < solution.timeslot_by_exam.size());
        const auto source = solution.timeslot_by_exam[move.exam];
        penalty_type change = 0;

        for (const auto& conflict : instance_.conflicts)
        {
            exam_id other{};
            if (conflict.first == move.exam)
            {
                other = conflict.second;
            }
            else if (conflict.second == move.exam)
            {
                other = conflict.first;
            }
            else
            {
                continue;
            }

            const auto other_timeslot = solution.timeslot_by_exam[other];
            const auto old_distance = source > other_timeslot
                ? source - other_timeslot
                : other_timeslot - source;
            const auto new_distance = move.destination > other_timeslot
                ? move.destination - other_timeslot
                : other_timeslot - move.destination;

            if (old_distance == 1)
            {
                change -= conflict.students;
            }
            if (new_distance == 1)
            {
                change += conflict.students;
            }
        }

        return change;
    }

private:
    const ExamTimetablingInstance& instance_;
};

class TimeslotLoadDeltaEvaluator
{
public:
    explicit TimeslotLoadDeltaEvaluator(
        const ExamTimetablingInstance& instance) noexcept
        : instance_{instance}
    {
    }

    [[nodiscard]]
    auto delta_evaluate(
        const ExamTimetable& solution,
        const MoveExam& move) const -> penalty_type
    {
        assert(move.exam < solution.timeslot_by_exam.size());
        const auto source = solution.timeslot_by_exam[move.exam];
        assert(source != move.destination);
        assert(move.destination < instance_.timeslot_count);

        std::vector<penalty_type> load(instance_.timeslot_count, 0);
        for (const auto timeslot : solution.timeslot_by_exam)
        {
            ++load[timeslot];
        }

        const auto source_load = load[source];
        const auto destination_load = load[move.destination];
        const auto before =
            source_load * source_load +
            destination_load * destination_load;
        const auto after =
            (source_load - 1) * (source_load - 1) +
            (destination_load + 1) * (destination_load + 1);

        return after - before;
    }

private:
    const ExamTimetablingInstance& instance_;
};

} // namespace easylocal::mwe::exam_timetabling
