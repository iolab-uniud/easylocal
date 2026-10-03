#pragma once

#include "cost_components.hpp"
#include "move.hpp"

#include <cassert>

namespace exam_timetabling
{

class ConsecutiveExamDeltaEvaluator
{
public:
    explicit ConsecutiveExamDeltaEvaluator(const ExamTimetablingInstance& instance)
        : instance_{instance}
    {
    }

    penalty_type delta_evaluate(const ExamTimetable& solution, const MoveExam& move) const
    {
        assert(move.exam < solution.timeslot_by_exam.size());
        const auto source = solution.timeslot_by_exam[move.exam];
        penalty_type change = 0;

        for (const auto& conflict : instance_.conflicts)
        {
            exam_id other{};
            if (conflict.first == move.exam)
                other = conflict.second;
            else if (conflict.second == move.exam)
                other = conflict.first;
            else
                continue;

            const auto other_timeslot = solution.timeslot_by_exam[other];
            const auto old_distance = source > other_timeslot
                ? source - other_timeslot
                : other_timeslot - source;
            const auto new_distance = move.destination > other_timeslot
                ? move.destination - other_timeslot
                : other_timeslot - move.destination;

            if (old_distance == 1)
                change -= conflict.students;
            if (new_distance == 1)
                change += conflict.students;
        }

        return change;
    }

private:
    const ExamTimetablingInstance& instance_;
};

// TimeslotLoadComponent has no delta evaluator: the change of a timeslot's
// load needs all the loads, and counting them visits every exam, which is what
// a full evaluation does. EasyLocal then evaluates each move on a candidate
// solution, which costs the same and needs no code.

} // namespace exam_timetabling
