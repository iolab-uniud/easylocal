#pragma once

#include "instance.hpp"

#include <cstddef>
#include <istream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace easylocal::mwe::assignment
{

using machine_id = std::size_t;

struct AssignmentSolution
{
    std::vector<machine_id> assignment;

    [[nodiscard]]
    static auto read(const AssignmentInstance& instance, std::istream& in)
        -> AssignmentSolution
    {
        AssignmentSolution solution{
            .assignment = std::vector<machine_id>(instance.demand.size()),
        };
        for (auto& machine : solution.assignment)
        {
            if (!(in >> machine))
            {
                throw std::runtime_error{"invalid assignment solution"};
            }
        }
        return solution;
    }

    void write(const AssignmentInstance&, std::ostream& out) const
    {
        for (std::size_t job = 0; job < assignment.size(); ++job)
        {
            if (job != 0)
            {
                out << ' ';
            }
            out << assignment[job];
        }
        out << '\n';
    }

    [[nodiscard]] auto describe() const -> std::string
    {
        std::ostringstream out;
        out << "jobs=" << assignment.size() << "  assignment=[";
        for (std::size_t job = 0; job < assignment.size(); ++job)
        {
            if (job != 0)
            {
                out << ", ";
            }
            out << job << "->" << assignment[job];
        }
        out << ']';
        return out.str();
    }
};

} // namespace easylocal::mwe::assignment
