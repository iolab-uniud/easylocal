#pragma once

#include "instance.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace pfsp
{

inline PfspInstance load_instance(const std::filesystem::path& path)
{
    std::ifstream input{path};
    if (!input)
        throw std::runtime_error("cannot open PFSP instance: " + path.string());

    try
    {
        return PfspInstance::read(input);
    }
    catch (const std::exception& error)
    {
        throw std::runtime_error(
            "cannot read PFSP instance '" + path.string() + "': " + error.what());
    }
}

} // namespace pfsp
