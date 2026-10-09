#pragma once

/// \file
/// input_base: the base of a class built from the Input, which gives it
/// input_type and input().

#include <memory>

namespace easylocal
{

/// A base for a class built from the Input, which gives it `input_type` and
/// `input()`.
///
/// Cost components, delta cost components and the SolutionManager and
/// neighborhood explorer bases derive from it, so that every class built on
/// the Input reaches it the same way. It is optional and non-virtual: a class
/// that declares `input()` itself does just as well, and a class that needs
/// no Input derives from nothing. A derived class inherits its constructor
/// with `using input_base::input_base;`, and derives from it once: a cost
/// component with a co-located delta is one class, with one Input.
template<class Input>
class input_base
{
public:
    /// The Input type.
    using input_type = Input;

    /// From the Input, which it keeps by reference: the Input must outlive the
    /// object.
    explicit input_base(const input_type& input) noexcept : input_{std::addressof(input)}
    {
    }

    /// Not from a temporary Input, which would dangle: the Input must outlive
    /// the object.
    explicit input_base(const input_type&&) = delete;

    /// The Input.
    [[nodiscard]]
    const input_type& input() const noexcept
    {
        return *input_;
    }

private:
    // A pointer, not a reference: the layers that compose these classes copy
    // and assign them.
    const input_type* input_;
};

} // namespace easylocal
