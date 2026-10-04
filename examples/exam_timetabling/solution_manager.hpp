#pragma once

// The solution manager of exam timetabling: the initial timetable and the
// validity check.

#include "instance.hpp"
#include "solution.hpp"

#include <easylocal/helpers/solution_manager.hpp>

#include <algorithm>
#include <cstddef>
#include <vector>

namespace exam_timetabling
{

class ExamTimetablingSolutionManager
    : public easylocal::solution_manager_base<ExamTimetablingInstance, ExamTimetable>
{
public:
    using solution_manager_base::solution_manager_base;

    // Exams spread over the timeslots in turn: exam e in timeslot e mod T.
    ExamTimetable initial_solution() const
    {
        ExamTimetable solution{
            .timeslot_by_exam = std::vector<timeslot_id>(input().exam_count),
        };
        for (std::size_t exam = 0; exam < solution.timeslot_by_exam.size(); ++exam)
            solution.timeslot_by_exam[exam] = exam % input().timeslot_count;
        return solution;
    }

    bool is_valid(const ExamTimetable& solution) const
    {
        return solution.timeslot_by_exam.size() == input().exam_count
            && std::ranges::all_of(
                solution.timeslot_by_exam,
                [this](timeslot_id timeslot) {
                    return timeslot < input().timeslot_count;
                });
    }
};

} // namespace exam_timetabling
