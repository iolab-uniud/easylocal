#pragma once

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <istream>
#include <stdexcept>
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

    // The text format: "exams timeslots conflicts", then one
    // "first second students" line per conflict.
    static ExamTimetablingInstance read(std::istream& input)
    {
        std::size_t exam_count{};
        std::size_t timeslot_count{};
        std::size_t conflict_count{};
        if (!(input >> exam_count >> timeslot_count >> conflict_count))
            throw std::runtime_error("invalid exam-timetabling instance header");

        ExamTimetablingInstance instance{
            .exam_count = exam_count,
            .timeslot_count = timeslot_count,
            .conflicts = {},
        };
        instance.conflicts.reserve(conflict_count);
        for (std::size_t index = 0; index < conflict_count; ++index)
        {
            ExamConflict conflict{};
            if (!(input >> conflict.first >> conflict.second >> conflict.students))
                throw std::runtime_error("invalid exam-timetabling conflict data");
            instance.conflicts.push_back(conflict);
        }

        if (!instance.is_valid())
            throw std::runtime_error("invalid exam-timetabling instance");
        return instance;
    }

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

// An exam that shares students with a given one.
struct ConflictingExam
{
    exam_id exam;
    penalty_type students;
};

// For each exam, the exams it shares students with: a delta cost component visits
// only the conflicts of the exam a move changes.
inline std::vector<std::vector<ConflictingExam>> conflicts_by_exam(
    const ExamTimetablingInstance& instance)
{
    std::vector<std::vector<ConflictingExam>> conflicts(instance.exam_count);
    for (const auto& conflict : instance.conflicts)
    {
        conflicts[conflict.first].push_back({conflict.second, conflict.students});
        conflicts[conflict.second].push_back({conflict.first, conflict.students});
    }
    return conflicts;
}

} // namespace exam_timetabling
