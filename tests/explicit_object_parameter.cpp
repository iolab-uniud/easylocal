// Whether every supported compiler has explicit object parameters (C++23,
// "deducing this"): one member giving both the const and the mutable version,
// planned for the const and non-const twins of App (docs/roadmap.md). A
// compiler without them fails to build this test.
#include <cassert>
#include <string>
#include <type_traits>
#include <utility>

#if !defined(__cpp_explicit_this_parameter) || __cpp_explicit_this_parameter < 202110L
#error "this compiler lacks explicit object parameters (__cpp_explicit_this_parameter)"
#endif

namespace
{

struct Settings
{
    int size = 1;
};

class Holder
{
public:
    // The shape of App::configuration() and runner_config(): a reference to a
    // member, const exactly when the object is.
    template<class Self>
    [[nodiscard]]
    auto& settings(this Self& self) noexcept
    {
        return self.settings_;
    }

    // A member template with its own parameters next to the object's.
    template<class Self>
    [[nodiscard]]
    std::string describe(this const Self& self, const std::string& prefix)
    {
        return prefix + std::to_string(self.settings_.size);
    }

private:
    Settings settings_;
};

static_assert(std::is_same_v<decltype(std::declval<Holder&>().settings()), Settings&>);
static_assert(
    std::is_same_v<decltype(std::declval<const Holder&>().settings()), const Settings&>);

} // namespace

int main()
{
    Holder holder;
    holder.settings().size = 3;
    const Holder& view = holder;
    assert(view.settings().size == 3);
    assert(view.describe("size ") == "size 3");
}
