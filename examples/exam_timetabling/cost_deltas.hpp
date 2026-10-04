#pragma once

// The delta cost component of ConsecutiveExamComponent, written as a class of
// its own.

#include "instance.hpp"
#include "move.hpp"
#include "solution.hpp"

#include <cassert>
#include <vector>

namespace exam_timetabling
{

class ConsecutiveExamDeltaEvaluator
{
public:
    explicit ConsecutiveExamDeltaEvaluator(const ExamTimetablingInstance& instance)
        : conflicts_by_exam_{conflicts_by_exam(instance)}
    {
    }

    penalty_type delta_evaluate(const ExamTimetable& solution, const MoveExam& move) const
    {
        assert(move.exam < solution.timeslot_by_exam.size());
        const auto source = solution.timeslot_by_exam[move.exam];
        penalty_type change = 0;

        for (const auto& conflict : conflicts_by_exam_[move.exam])
        {
            const auto other_timeslot = solution.timeslot_by_exam[conflict.exam];
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
    std::vector<std::vector<ConflictingExam>> conflicts_by_exam_;
};

// TimeslotLoadComponent has no delta cost component: the change of a timeslot's
// load needs all the loads, and counting them visits every exam, which is what
// a full evaluation does. EasyLocal then evaluates each move on a candidate
// solution, which costs the same and needs no code.

} // namespace exam_timetabling
