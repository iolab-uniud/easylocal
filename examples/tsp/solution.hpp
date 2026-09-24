#pragma once

#include "instance.hpp"

#include <vector>

namespace easylocal::mwe::tsp
{

struct Solution
{
    std::vector<city_id> tour;
};

} // namespace easylocal::mwe::tsp
