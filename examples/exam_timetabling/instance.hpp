#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace exam_timetabling
{

using exam_id = std::size_t;
using timeslot_id = std::size_t;
using penalty_type = std::int64_t;

struct ExamConflict
{
    exam_id first;
    exam_id second;
    penalty_type students;
};

struct ExamTimetablingInstance
{
    std::size_t exam_count{};
    std::size_t timeslot_count{};
    std::vector<ExamConflict> conflicts;

    bool is_valid() const
    {
        if (timeslot_count == 0)
            return exam_count == 0 && conflicts.empty();

        for (const auto& conflict : conflicts)
        {
            if (conflict.first >= exam_count || conflict.second >= exam_count
                || conflict.first == conflict.second || conflict.students < 0)
            {
                return false;
            }
        }

        return true;
    }
};

} // namespace exam_timetabling
