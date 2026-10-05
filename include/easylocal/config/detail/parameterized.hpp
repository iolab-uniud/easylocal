#pragma once

// The parameter rule: a class whose parameters_type is a parameter block is
// configurable at the path of its role, and constructed from that block. What
// a recipe holds of such a class's parameters, how they lead its construction
// arguments, and the check that a class declares its parameters this way.

#include <easylocal/config/parameter_set.hpp>
#include <easylocal/config/parameters.hpp>

#include <concepts>
#include <tuple>
#include <type_traits>
#include <utility>

namespace easylocal::config::detail
{

// A class configured by its parameters_type, a parameter block.
template<class T>
concept parameterized = requires {
    typename T::parameters_type;
} && parameter_block<typename T::parameters_type>;

// A class that has parameters without declaring them as its parameters_type:
// a parameters() that returns a parameter block, or a configuration().
template<class T>
concept undeclared_parameters = !parameterized<T>
    && (requires(const T& value) {
           requires parameter_block<std::remove_cvref_t<decltype(value.parameters())>>;
       } || requires(T& value) {
           { value.configuration() } -> std::same_as<parameter_set>;
       });

// Only a parameters_type is configured: a class with parameters() or
// configuration() and no parameter-block parameters_type does not compile.
template<class T>
consteval bool check_declared_parameters()
{
    static_assert(
        !undeclared_parameters<T>,
        "only a parameters_type is configured: declare `using parameters_type = "
        "<its parameter block>;` and a constructor that takes the block (after the "
        "Input or the SolutionManager it is built from, if any), instead of "
        "parameters() or configuration()");
    return true;
}

// The parameters of a class without parameters.
struct no_parameters
{
};

// The parameters_type of a parameterized class, no_parameters otherwise.
template<class T>
struct parameters_storage
{
    using type = no_parameters;
};

template<parameterized T>
struct parameters_storage<T>
{
    using type = typename T::parameters_type;
};

template<class T>
using parameters_storage_t = typename parameters_storage<T>::type;

// The arguments T is constructed with after its Input or SolutionManager: its
// parameters, when it has any, then Args.
template<class T, class Arguments>
struct construction_arguments
{
    using type = Arguments;
};

template<parameterized T, class... Args>
struct construction_arguments<T, std::tuple<Args...>>
{
    using type = std::tuple<typename T::parameters_type, Args...>;
};

template<class T, class Arguments>
using construction_arguments_t = typename construction_arguments<T, Arguments>::type;

// What a recipe holds of the parameters of T: nothing, for a class without
// parameters.
template<class T>
class parameters_holder
{
    static_assert(check_declared_parameters<T>());

public:
    using parameters_type = no_parameters;

    parameters_holder() = default;

    explicit parameters_holder(no_parameters) noexcept {}

    // A reference, as the holder of real parameters gives one.
    [[nodiscard]]
    const no_parameters& parameters() const noexcept
    {
        static constexpr no_parameters none{};
        return none;
    }

    // The arguments of T's construction: args, by reference.
    template<class... Args>
    [[nodiscard]]
    std::tuple<const Args&...> arguments(const std::tuple<Args...>& args) const
    {
        return std::apply(
            [](const Args&... arg) { return std::tuple<const Args&...>{arg...}; },
            args);
    }
};

// The parameters of a parameterized T: a configurable endpoint, whose
// parameters lead T's construction arguments.
template<parameterized T>
class parameters_holder<T>
{
public:
    using parameters_type = typename T::parameters_type;

    // Throws std::invalid_argument when the default parameters are not valid.
    parameters_holder()
        requires std::default_initializable<parameters_type>
        : parameters_holder{parameters_type{}}
    {
    }

    // Throws std::invalid_argument when the parameters are not valid.
    explicit parameters_holder(parameters_type parameters)
        : parameters_{std::move(parameters)}
    {
        require_valid(parameters_);
    }

    template<class Self>
    [[nodiscard]]
    auto& parameters(this Self& self) noexcept
    {
        return self.parameters_;
    }

    [[nodiscard]]
    validation_result configure(parameters_type parameters)
    {
        const auto validation = parameters.validate();
        if (!validation)
            return validation;
        parameters_ = std::move(parameters);
        return validation_result::success();
    }

    // The arguments of T's construction: the parameters, then args, by
    // reference. Throws std::invalid_argument when the parameters, which
    // parameters() may have changed, are not valid.
    template<class... Args>
    [[nodiscard]]
    std::tuple<const parameters_type&, const Args&...> arguments(
        const std::tuple<Args...>& args) const
    {
        require_valid(parameters_);
        return std::apply(
            [this](const Args&... arg) {
                return std::tuple<const parameters_type&, const Args&...>{
                    parameters_,
                    arg...};
            },
            args);
    }

private:
    parameters_type parameters_;
};

} // namespace easylocal::config::detail
