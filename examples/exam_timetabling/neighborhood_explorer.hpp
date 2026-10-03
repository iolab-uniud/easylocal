#pragma once

#include "move.hpp"
#include "solution_manager.hpp"

#include <easylocal/helpers/neighborhood_explorer.hpp>

#include <cassert>
#include <cstddef>
#include <optional>
#include <random>

namespace exam_timetabling
{

class MoveExamNeighborhoodExplorer
    : public easylocal::neighborhood_explorer_base<
          ExamTimetablingSolutionManager,
          MoveExam>
{
public:
    using neighborhood_explorer_base::neighborhood_explorer_base;

    bool is_valid(const ExamTimetable& solution, const MoveExam& move) const
    {
        return move.exam < solution.timeslot_by_exam.size()
            && move.destination < input().timeslot_count
            && solution.timeslot_by_exam[move.exam] != move.destination;
    }

    bool first_move(const ExamTimetable& solution, MoveExam& move) const
    {
        if (solution.timeslot_by_exam.empty() || input().timeslot_count < 2)
            return false;

        move.exam = 0;
        move.destination = first_destination(solution, 0);
        return true;
    }

    bool next_move(const ExamTimetable& solution, MoveExam& move) const
    {
        const auto current = solution.timeslot_by_exam[move.exam];
        for (auto destination = move.destination + 1;
            destination < input().timeslot_count;
            ++destination)
        {
            if (destination != current)
            {
                move.destination = destination;
                return true;
            }
        }

        if (move.exam + 1 < solution.timeslot_by_exam.size())
        {
            ++move.exam;
            move.destination = first_destination(solution, move.exam);
            return true;
        }

        return false;
    }

    template<std::uniform_random_bit_generator RNG>
    std::optional<MoveExam> random_move(const ExamTimetable& solution, RNG& rng) const
    {
        const auto alternatives =
            input().timeslot_count > 0 ? input().timeslot_count - 1 : std::size_t{0};
        const auto count = solution.timeslot_by_exam.size() * alternatives;

        if (count == 0)
            return std::nullopt;

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

    void make_move(ExamTimetable& solution, const MoveExam& move) const
    {
        solution.timeslot_by_exam[move.exam] = move.destination;
    }

private:
    timeslot_id first_destination(const ExamTimetable& solution, exam_id exam) const
    {
        return solution.timeslot_by_exam[exam] == 0 ? timeslot_id{1} : timeslot_id{0};
    }
};

} // namespace exam_timetabling
