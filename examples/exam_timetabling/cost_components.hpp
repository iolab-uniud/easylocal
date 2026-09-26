#pragma once

#include "instance.hpp"
#include "solution.hpp"

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <vector>

namespace easylocal::mwe::exam_timetabling
{

struct StudentConflictValue
{
    penalty_type penalty{};
    auto operator==(const StudentConflictValue&) const -> bool = default;

    [[nodiscard]]
    friend constexpr auto operator*(
        const penalty_type weight,
        const StudentConflictValue value) noexcept -> penalty_type
    {
        return weight * value.penalty;
    }
};

struct ConsecutiveExamValue
{
    penalty_type penalty{};
    auto operator==(const ConsecutiveExamValue&) const -> bool = default;

    [[nodiscard]]
    friend constexpr auto operator*(
        const penalty_type weight,
        const ConsecutiveExamValue value) noexcept -> penalty_type
    {
        return weight * value.penalty;
    }
};

struct TimeslotLoadValue
{
    penalty_type penalty{};
    auto operator==(const TimeslotLoadValue&) const -> bool = default;

    [[nodiscard]]
    friend constexpr auto operator*(
        const penalty_type weight,
        const TimeslotLoadValue value) noexcept -> penalty_type
    {
        return weight * value.penalty;
    }
};

class StudentConflictComponent
{
public:
    explicit StudentConflictComponent(
        const ExamTimetablingInstance& instance) noexcept
        : instance_{instance}
    {
    }

    [[nodiscard]]
    auto evaluate(const ExamTimetable& solution) const -> StudentConflictValue
    {
        assert(solution.timeslot_by_exam.size() == instance_.exam_count);
        penalty_type penalty = 0;

        for (const auto& conflict : instance_.conflicts)
        {
            if (solution.timeslot_by_exam[conflict.first] ==
                solution.timeslot_by_exam[conflict.second])
            {
                penalty += conflict.students;
            }
        }

        return {.penalty = penalty};
    }

private:
    const ExamTimetablingInstance& instance_;
};

class ConsecutiveExamComponent
{
public:
    explicit ConsecutiveExamComponent(
        const ExamTimetablingInstance& instance) noexcept
        : instance_{instance}
    {
    }

    [[nodiscard]]
    auto evaluate(const ExamTimetable& solution) const -> ConsecutiveExamValue
    {
        assert(solution.timeslot_by_exam.size() == instance_.exam_count);
        penalty_type penalty = 0;

        for (const auto& conflict : instance_.conflicts)
        {
            const auto first = solution.timeslot_by_exam[conflict.first];
            const auto second = solution.timeslot_by_exam[conflict.second];
            const auto distance = first > second ? first - second : second - first;

            if (distance == 1)
            {
                penalty += conflict.students;
            }
        }

        return {.penalty = penalty};
    }

private:
    const ExamTimetablingInstance& instance_;
};

class TimeslotLoadComponent
{
public:
    explicit TimeslotLoadComponent(
        const ExamTimetablingInstance& instance) noexcept
        : instance_{instance}
    {
    }

    [[nodiscard]]
    auto evaluate(const ExamTimetable& solution) const -> TimeslotLoadValue
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
        {
            penalty += count * count;
        }

        return {.penalty = penalty};
    }

private:
    const ExamTimetablingInstance& instance_;
};

} // namespace easylocal::mwe::exam_timetabling
