#pragma once

#include "instance.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace exam_timetabling
{

inline ExamTimetablingInstance load_instance(const std::filesystem::path& path)
{
    std::ifstream input{path};
    if (!input)
    {
        throw std::runtime_error(
            "cannot open exam-timetabling instance: " + path.string());
    }

    std::size_t exam_count{};
    std::size_t timeslot_count{};
    std::size_t conflict_count{};
    if (!(input >> exam_count >> timeslot_count >> conflict_count))
    {
        throw std::runtime_error(
            "invalid exam-timetabling instance header: " + path.string());
    }

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
        {
            throw std::runtime_error(
                "invalid exam-timetabling conflict data: " + path.string());
        }
        instance.conflicts.push_back(conflict);
    }

    if (!instance.is_valid())
    {
        throw std::runtime_error("invalid exam-timetabling instance: " + path.string());
    }

    return instance;
}

} // namespace exam_timetabling
