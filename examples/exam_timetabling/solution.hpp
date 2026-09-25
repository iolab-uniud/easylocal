#pragma once

#include "instance.hpp"

#include <vector>

namespace easylocal::mwe::exam_timetabling
{

struct ExamTimetable
{
    std::vector<timeslot_id> timeslot_by_exam;

    auto operator==(const ExamTimetable&) const -> bool = default;
};

} // namespace easylocal::mwe::exam_timetabling
