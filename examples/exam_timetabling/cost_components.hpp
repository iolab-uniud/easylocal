#pragma once

#include "instance.hpp"
#include "move.hpp"
#include "solution.hpp"

#include <cassert>
#include <cstddef>
#include <vector>

namespace exam_timetabling
{

class StudentConflictComponent
{
public:
    explicit StudentConflictComponent(const ExamTimetablingInstance& instance)
        : instance_{instance}
    {
    }

    penalty_type evaluate(const ExamTimetable& solution) const
    {
        assert(solution.timeslot_by_exam.size() == instance_.exam_count);
        penalty_type penalty = 0;

        for (const auto& conflict : instance_.conflicts)
        {
            if (solution.timeslot_by_exam[conflict.first]
                == solution.timeslot_by_exam[conflict.second])
            {
                penalty += conflict.students;
            }
        }

        return penalty;
    }

    // A delta evaluator may be co-located with its component when that is the
    // clearest expression. Separate evaluator types remain the primary model.
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
            if (source == other_timeslot)
                change -= conflict.students;
            if (move.destination == other_timeslot)
                change += conflict.students;
        }

        return change;
    }

private:
    const ExamTimetablingInstance& instance_;
};

class ConsecutiveExamComponent
{
public:
    explicit ConsecutiveExamComponent(const ExamTimetablingInstance& instance)
        : instance_{instance}
    {
    }

    penalty_type evaluate(const ExamTimetable& solution) const
    {
        assert(solution.timeslot_by_exam.size() == instance_.exam_count);
        penalty_type penalty = 0;

        for (const auto& conflict : instance_.conflicts)
        {
            const auto first = solution.timeslot_by_exam[conflict.first];
            const auto second = solution.timeslot_by_exam[conflict.second];
            const auto distance = first > second ? first - second : second - first;

            if (distance == 1)
                penalty += conflict.students;
        }

        return penalty;
    }

private:
    const ExamTimetablingInstance& instance_;
};

class TimeslotLoadComponent
{
public:
    explicit TimeslotLoadComponent(const ExamTimetablingInstance& instance)
        : instance_{instance}
    {
    }

    penalty_type evaluate(const ExamTimetable& solution) const
    {
        assert(solution.timeslot_by_exam.size() == instance_.exam_count);
        std::vector<penalty_type> load(instance_.timeslot_count, 0);

        for (const auto timeslot : solution.timeslot_by_exam)
        {
            assert(timeslot < instance_.timeslot_count);
            ++load[timeslot];
        }

        penalty_type penalty = 0;
        for (const auto count : load)
            penalty += count * count;

        return penalty;
    }

private:
    const ExamTimetablingInstance& instance_;
};

} // namespace exam_timetabling
