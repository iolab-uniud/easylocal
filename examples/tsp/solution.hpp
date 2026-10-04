#pragma once

// The TSP Solution: a tour, the order in which the cities are visited.

#include "instance.hpp"

#include <istream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace tsp
{

struct Tour
{
    std::vector<city_id> tour;

    static Tour read(const TspInstance& instance, std::istream& input)
    {
        Tour solution;
        solution.tour.resize(instance.city_count);
        for (auto& city : solution.tour)
            if (!(input >> city))
                throw std::runtime_error("invalid TSP solution");
        return solution;
    }

    void write(const TspInstance&, std::ostream& output) const
    {
        for (std::size_t index = 0; index < tour.size(); ++index)
        {
            if (index != 0)
                output << ' ';
            output << tour[index];
        }
        output << '\n';
    }

    bool operator==(const Tour&) const = default;

    std::string describe() const
    {
        std::ostringstream output;
        output << "tour: ";
        for (std::size_t index = 0; index < tour.size(); ++index)
        {
            if (index != 0)
                output << " -> ";
            output << tour[index];
        }
        return output.str();
    }
};

} // namespace tsp
