#pragma once

/// \file
/// run_services: the state a run needs on the problem's side, declared by the
/// SolutionManager and built once when the app, the runner or the fixture is
/// bound to an Input.

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

namespace easylocal
{

/// A SolutionManager that declares the services of a run: the state its cost
/// components, delta cost components and explorers share while they search.
///
/// `using services_type = T;` on the SolutionManager is the declaration. One T
/// is built for each bind, from the Input when T takes one and otherwise
/// without arguments, and handed to whatever asks for it: a component
/// constructible from `(const Input&, T&, ...)` is built that way, as are a
/// delta cost component and an explorer. A cache, the loads of a solution, the
/// network of a declared model are what T holds; it is never shared between
/// two binds, so two runs never share it.
template<class SM>
concept has_run_services = requires { typename SM::services_type; };

/// The services of a run of `SM`, which declares them.
template<has_run_services SM>
using run_services_t = typename SM::services_type;

namespace detail
{

// Whether a layer of a SolutionManager carries the services of a run: the one
// question asked before the construction of a component, a delta or an
// explorer is probed against them, so that a problem without services pays no
// instantiation for them.
template<class Dependency>
concept carries_run_services = requires(const Dependency& dependency) {
    dependency.services();
};

// The services of a run, or nothing when the SolutionManager declares none.
//
// They are held behind a pointer, and never copied: the layers of a
// SolutionManager are moved as they are composed, and what a component keeps
// must not move with them. A SolutionManager with services is therefore
// move-only, which is also what keeps two runs from sharing them.
template<class SM, class Input, bool = has_run_services<SM>>
class run_services_holder
{
public:
    static constexpr bool present = false;

    explicit run_services_holder(const Input&) noexcept {}
};

template<class SM, class Input>
class run_services_holder<SM, Input, true>
{
public:
    static constexpr bool present = true;

    using services_type = run_services_t<SM>;

    explicit run_services_holder(const Input& input) : services_{build(input)} {}

    // The services are the mutable state of a run: a const SolutionManager
    // hands them out as it hands out the Input, and what they hold changes.
    [[nodiscard]]
    services_type& services() const noexcept
    {
        return *services_;
    }

private:
    static std::unique_ptr<services_type> build(const Input& input)
    {
        if constexpr (std::constructible_from<services_type, const Input&>)
        {
            return std::make_unique<services_type>(input);
        }
        else
        {
            static_assert(
                std::default_initializable<services_type>,
                "the services_type of a SolutionManager must be constructible "
                "from the bound Input or without arguments");
            return std::make_unique<services_type>();
        }
    }

    std::unique_ptr<services_type> services_;
};

} // namespace detail

} // namespace easylocal
