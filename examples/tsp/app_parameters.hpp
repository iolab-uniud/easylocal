#pragma once

#include <easylocal/config/parameters.hpp>

#include <cstdint>
#include <filesystem>

namespace easylocal::mwe::tsp
{

struct AppParameters
{
    std::filesystem::path instance_file{"instance.tsp"};
    std::uint64_t seed{2026U};

    [[nodiscard]]
    static consteval auto parameter_schema()
    {
        return config::fields(
            config::field<
                "instance_file",
                &AppParameters::instance_file>(
                    "Problem instance file"),
            config::field<
                "seed",
                &AppParameters::seed>(
                    "Pseudo-random generator seed"));
    }

    [[nodiscard]]
    auto validate() const noexcept -> config::validation_result
    {
        if (instance_file.empty())
        {
            return config::validation_result::failure(
                "instance_file must not be empty");
        }

        return config::validation_result::success();
    }
};

} // namespace easylocal::mwe::tsp
