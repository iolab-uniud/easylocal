#pragma once

/// \file
/// parameters_base: the base of a class configured by a parameter block,
/// which gives it parameters_type and parameters().

#include <easylocal/config/parameters.hpp>

#include <type_traits>
#include <utility>

namespace easylocal
{

/// A base for a class configured by a parameter block, which gives it
/// `parameters_type` and `parameters()`.
///
/// Declaring `parameters_type` is what makes a class configurable, so a class
/// that derives from this one is configured at the path of its role, with no
/// further declaration; a recipe holds the block, validates it and passes it
/// to the constructor, after the Input or the SolutionManager the class is
/// built from. It is optional and non-virtual: a class that declares
/// `parameters_type` and takes the block itself does just as well. A derived
/// class inherits its constructor with
/// `using parameters_base::parameters_base;`.
///
/// It does not validate the block: the recipe that holds it does, before every
/// construction.
template<config::parameter_block Parameters>
class parameters_base
{
public:
    /// The parameter block of the class.
    using parameters_type = Parameters;

    /// From its parameters.
    explicit parameters_base(parameters_type parameters) noexcept(
        std::is_nothrow_move_constructible_v<parameters_type>)
        : parameters_{std::move(parameters)}
    {
    }

    /// The parameters.
    [[nodiscard]]
    const parameters_type& parameters() const noexcept
    {
        return parameters_;
    }

private:
    parameters_type parameters_;
};

} // namespace easylocal
