#include <easylocal/sampling.hpp>

#include <concepts>

int main()
{
    static_assert(!std::same_as<
        easylocal::sampling::with_replacement,
        easylocal::sampling::without_replacement>);
    return 0;
}
