#pragma once

#include "instance.hpp"

namespace easylocal::mwe::exam_timetabling
{

struct MoveExam
{
    exam_id exam;
    timeslot_id destination;

    auto operator==(const MoveExam&) const -> bool = default;
};

} // namespace easylocal::mwe::exam_timetabling
