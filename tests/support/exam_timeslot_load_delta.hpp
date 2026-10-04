#pragma once

// A delta cost component for the exam-timetabling example's timeslot load, used by
// the tests as a third delta bound to the same neighborhood. It is not part of
// the example: counting the loads visits every exam, so it costs as much as
// the full evaluation EasyLocal falls back to.
#include "../../examples/exam_timetabling/cost_components.hpp"
#include "../../examples/exam_timetabling/move.hpp"

#include <cassert>
#include <vector>

namespace exam_timetabling
{

class TimeslotLoadDeltaEvaluator
{
public:
    explicit TimeslotLoadDeltaEvaluator(const ExamTimetablingInstance& instance)
        : instance_{instance}
    {
    }

    penalty_type delta_evaluate(const ExamTimetable& solution, const MoveExam& move) const
    {
        assert(move.exam < solution.timeslot_by_exam.size());
        const auto source = solution.timeslot_by_exam[move.exam];
        assert(source != move.destination);
        assert(move.destination < instance_.timeslot_count);

        std::vector<penalty_type> load(instance_.timeslot_count, 0);
        for (const auto timeslot : solution.timeslot_by_exam)
        {
            ++load[timeslot];
        }

        const auto source_load = load[source];
        const auto destination_load = load[move.destination];
        const auto before =
            source_load * source_load + destination_load * destination_load;
        const auto after = (source_load - 1) * (source_load - 1)
            + (destination_load + 1) * (destination_load + 1);

        return after - before;
    }

private:
    const ExamTimetablingInstance& instance_;
};

} // namespace exam_timetabling
