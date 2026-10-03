#pragma once

#include "instance.hpp"

#include <cstddef>
#include <istream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace exam_timetabling
{

struct ExamTimetable
{
    std::vector<timeslot_id> timeslot_by_exam;

    // The timeslot of each exam, in exam order.
    static ExamTimetable read(
        const ExamTimetablingInstance& instance,
        std::istream& input)
    {
        ExamTimetable solution;
        solution.timeslot_by_exam.resize(instance.exam_count);
        for (auto& timeslot : solution.timeslot_by_exam)
            if (!(input >> timeslot) || timeslot >= instance.timeslot_count)
                throw std::runtime_error("invalid exam timetable");
        return solution;
    }

    void write(const ExamTimetablingInstance&, std::ostream& output) const
    {
        for (std::size_t exam = 0; exam < timeslot_by_exam.size(); ++exam)
        {
            if (exam != 0)
                output << ' ';
            output << timeslot_by_exam[exam];
        }
        output << '\n';
    }

    std::string describe() const
    {
        std::ostringstream output;
        output << "timetable: [";
        for (std::size_t exam = 0; exam < timeslot_by_exam.size(); ++exam)
        {
            if (exam != 0)
                output << ", ";
            output << timeslot_by_exam[exam];
        }
        output << ']';
        return output.str();
    }

    bool operator==(const ExamTimetable&) const = default;
};

} // namespace exam_timetabling
