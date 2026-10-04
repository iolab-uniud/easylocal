#pragma once

// The neighborhood explorer of exam timetabling: move one exam to another
// timeslot.

#include "move.hpp"
#include "solution_manager.hpp"

#include <easylocal/helpers/neighborhood_explorer.hpp>
#include <easylocal/utils/generator.hpp>

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

    // Every exam to every timeslot other than its own, in exam order.
    easylocal::generator<MoveExam> moves(const ExamTimetable& solution) const
    {
        for (exam_id exam = 0; exam < solution.timeslot_by_exam.size(); ++exam)
            for (timeslot_id timeslot = 0; timeslot < input().timeslot_count; ++timeslot)
                if (timeslot != solution.timeslot_by_exam[exam])
                    co_yield MoveExam{.exam = exam, .destination = timeslot};
    }

    bool is_valid(const ExamTimetable& solution, const MoveExam& move) const
    {
        return move.exam < solution.timeslot_by_exam.size()
            && move.destination < input().timeslot_count
            && solution.timeslot_by_exam[move.exam] != move.destination;
    }

    // One of the moves, drawn uniformly; none when there is no move.
    template<std::uniform_random_bit_generator RNG>
    std::optional<MoveExam> random_move(const ExamTimetable& solution, RNG& rng) const
    {
        const auto timeslot_count = input().timeslot_count;
        const auto alternatives =
            timeslot_count > 0 ? timeslot_count - 1 : std::size_t{0};
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
};

} // namespace exam_timetabling
