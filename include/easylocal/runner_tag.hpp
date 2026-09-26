#pragma once

#include <concepts>
#include <utility>

namespace easylocal::runner
{

template<class Algorithm, class Config>
    requires std::constructible_from<Algorithm, Config>
struct algorithm_tag
{
    using config_type = Config;

    [[nodiscard]]
    static auto make(config_type config) -> Algorithm
    {
        return Algorithm{std::move(config)};
    }
};

} // namespace easylocal::runner
