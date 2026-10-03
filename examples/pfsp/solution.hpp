#pragma once

#include "instance.hpp"

#include <istream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace pfsp
{

// The order in which the jobs are processed.
struct Schedule
{
    std::vector<job_id> order;

    static Schedule read(const PfspInstance& instance, std::istream& input)
    {
        Schedule solution;
        solution.order.resize(instance.job_count);
        for (auto& job : solution.order)
            if (!(input >> job))
                throw std::runtime_error("invalid PFSP solution");
        return solution;
    }

    void write(const PfspInstance&, std::ostream& output) const
    {
        for (std::size_t position = 0; position < order.size(); ++position)
        {
            if (position != 0)
                output << ' ';
            output << order[position];
        }
        output << '\n';
    }

    bool operator==(const Schedule&) const = default;

    std::string describe() const
    {
        std::ostringstream output;
        output << "order: ";
        for (std::size_t position = 0; position < order.size(); ++position)
        {
            if (position != 0)
                output << ' ';
            output << order[position];
        }
        return output.str();
    }
};

} // namespace pfsp
