#pragma once

// The cost components of exam timetabling: the students with two exams at the
// same time, those with exams in consecutive timeslots, and the timeslot loads.

#include "instance.hpp"
#include "move.hpp"
#include "solution.hpp"

#include <cassert>
#include <string_view>
#include <vector>

namespace exam_timetabling
{

class StudentConflictComponent
{
public:
    // The name of the component in reports (--report).
    static std::string_view name()
    {
        return "StudentConflicts";
    }

    explicit StudentConflictComponent(const ExamTimetablingInstance& instance)
        : instance_{instance}, conflicts_by_exam_{conflicts_by_exam(instance)}
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

    // The delta cost component, written as a member: the change of the penalty
    // when the move is made, from the conflicts of the moved exam only.
    penalty_type delta_evaluate(const ExamTimetable& solution, const MoveExam& move) const
    {
        assert(move.exam < solution.timeslot_by_exam.size());
        const auto source = solution.timeslot_by_exam[move.exam];
        penalty_type change = 0;

        for (const auto& conflict : conflicts_by_exam_[move.exam])
        {
            const auto other_timeslot = solution.timeslot_by_exam[conflict.exam];
            if (source == other_timeslot)
                change -= conflict.students;
            if (move.destination == other_timeslot)
                change += conflict.students;
        }

        return change;
    }

private:
    const ExamTimetablingInstance& instance_;
    std::vector<std::vector<ConflictingExam>> conflicts_by_exam_;
};

class ConsecutiveExamComponent
{
public:
    // The name of the component in reports (--report).
    static std::string_view name()
    {
        return "ConsecutiveExams";
    }

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
    // The name of the component in reports (--report).
    static std::string_view name()
    {
        return "TimeslotLoad";
    }

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
