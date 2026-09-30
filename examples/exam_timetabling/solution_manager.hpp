#pragma once

#include "cost_components.hpp"
#include "instance.hpp"
#include "solution.hpp"

#include <easylocal/service_base.hpp>

#include <algorithm>
#include <cstdint>

namespace easylocal::mwe::exam_timetabling
{

class ExamTimetablingSolutionManager
    : public easylocal::solution_manager_base<
          ExamTimetablingInstance,
          ExamTimetable>
{
public:
    using solution_manager_base::solution_manager_base;

    [[nodiscard]]
    auto is_valid(const ExamTimetable& solution) const noexcept -> bool
    {
        return solution.timeslot_by_exam.size() == input_.exam_count &&
               std::ranges::all_of(
                   solution.timeslot_by_exam,
                   [this](const timeslot_id timeslot) {
                       return timeslot < input_.timeslot_count;
                   });
    }

};

} // namespace easylocal::mwe::exam_timetabling
