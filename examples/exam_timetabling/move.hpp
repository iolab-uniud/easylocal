#pragma once

#include "instance.hpp"

namespace exam_timetabling
{

struct MoveExam
{
    exam_id exam;
    timeslot_id destination;

    bool operator==(const MoveExam&) const = default;
};

} // namespace exam_timetabling
