#include <easylocal/easylocal.hpp>

#include <array>
#include <concepts>
#include <ranges>
#include <span>

namespace
{

template <typename T>
concept Integer = std::integral<T>;

constexpr auto sum(std::span<const int> values) -> int
{
    int result = 0;

    for (const int value : values)
    {
        result += value;
    }

    return result;
}

} // namespace

static_assert(Integer<int>);
static_assert(__cplusplus >= 202302L);

int main()
{
    constexpr std::array values{1, 2, 3, 4};

    auto even = values | std::views::filter([](const int value) {
        return value % 2 == 0;
    });

    int filtered_sum = 0;

    for (const int value : even)
    {
        filtered_sum += value;
    }

    return (sum(values) == 10 && filtered_sum == 6) ? 0 : 1;
}
