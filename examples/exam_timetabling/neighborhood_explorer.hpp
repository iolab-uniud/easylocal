#pragma once

#include "move.hpp"
#include "solution_manager.hpp"

#include <easylocal/service_base.hpp>

#include <cassert>
#include <cstddef>
#include <optional>
#include <random>

namespace easylocal::mwe::exam_timetabling
{

class MoveExamNeighborhoodExplorer
    : public easylocal::neighborhood_explorer_base<
          ExamTimetablingSolutionManager,
          MoveExam>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    [[nodiscard]]
    auto is_valid(
        const ExamTimetable& solution,
        const MoveExam& move) const noexcept -> bool
    {
        return move.exam < solution.timeslot_by_exam.size() &&
               move.destination < instance().timeslot_count &&
               solution.timeslot_by_exam[move.exam] != move.destination;
    }

    [[nodiscard]]
    auto first_move(
        const ExamTimetable& solution,
        MoveExam& move) const noexcept -> bool
    {
        assert(solution_manager_.is_valid(solution));
        if (solution.timeslot_by_exam.empty() || instance().timeslot_count < 2)
        {
            return false;
        }

        move.exam = 0;
        move.destination = first_destination(solution, 0);
        return true;
    }

    [[nodiscard]]
    auto next_move(
        const ExamTimetable& solution,
        MoveExam& move) const noexcept -> bool
    {
        assert(is_valid(solution, move));

        const auto current = solution.timeslot_by_exam[move.exam];
        for (auto destination = move.destination + 1;
             destination < instance().timeslot_count;
             ++destination)
        {
            if (destination != current)
            {
                move.destination = destination;
                return true;
            }
        }

        for (auto exam = move.exam + 1;
             exam < solution.timeslot_by_exam.size();
             ++exam)
        {
            move.exam = exam;
            move.destination = first_destination(solution, exam);
            return true;
        }

        return false;
    }

    template<std::uniform_random_bit_generator RNG>
    [[nodiscard]]
    auto random_move(
        const ExamTimetable& solution,
        RNG& rng) const -> std::optional<MoveExam>
    {
        assert(solution_manager_.is_valid(solution));
        const auto alternatives = instance().timeslot_count > 0
            ? instance().timeslot_count - 1
            : std::size_t{0};
        const auto count = solution.timeslot_by_exam.size() * alternatives;

        if (count == 0)
        {
            return std::nullopt;
        }

        std::uniform_int_distribution<std::size_t> draw{0, count - 1};
        const auto ordinal = draw(rng);
        const auto exam = ordinal / alternatives;
        const auto offset = ordinal % alternatives;
        const auto current = solution.timeslot_by_exam[exam];
        const auto destination = offset < current ? offset : offset + 1;

        return MoveExam{
            .exam = exam,
            .destination = destination,
        };
    }

    void make_move(
        ExamTimetable& solution,
        const MoveExam& move) const noexcept
    {
        assert(is_valid(solution, move));
        solution.timeslot_by_exam[move.exam] = move.destination;
    }

private:
    [[nodiscard]]
    auto first_destination(
        const ExamTimetable& solution,
        const exam_id exam) const noexcept -> timeslot_id
    {
        return solution.timeslot_by_exam[exam] == 0
            ? timeslot_id{1}
            : timeslot_id{0};
    }
};

} // namespace easylocal::mwe::exam_timetabling
