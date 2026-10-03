#pragma once

#include "instance.hpp"

#include <vector>

namespace exam_timetabling
{

struct ExamTimetable
{
    std::vector<timeslot_id> timeslot_by_exam;

    bool operator==(const ExamTimetable&) const = default;
};

} // namespace exam_timetabling
