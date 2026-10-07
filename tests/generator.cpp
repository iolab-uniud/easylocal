// easylocal::generator: std::generator where the standard library has it,
// the fallback of utils/generator.hpp otherwise (libc++).

#include "support/expect.hpp"

#include <easylocal/utils/generator.hpp>

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{

easylocal::generator<int> count_to(int last)
{
    for (int value = 1; value <= last; ++value)
        co_yield value * 10;
}

// Yields the same lvalue twice: the consumer moves from the first value, and
// the coroutine still has its own.
easylocal::generator<std::string> lvalue_twice(std::string& seen)
{
    std::string text = "a text longer than the small string buffer";
    co_yield text;
    seen = text;
    co_yield text;
}

easylocal::generator<std::unique_ptr<int>> owned(int count)
{
    for (int value = 0; value < count; ++value)
        co_yield std::make_unique<int>(value);
}

easylocal::generator<int> throw_after(int count)
{
    for (int value = 0; value < count; ++value)
        co_yield value;
    throw std::runtime_error{"from the coroutine"};
}

class destruction_counter
{
public:
    explicit destruction_counter(int* destroyed) noexcept : destroyed_{destroyed} {}

    destruction_counter(const destruction_counter&) = delete;
    destruction_counter& operator=(const destruction_counter&) = delete;

    ~destruction_counter()
    {
        ++*destroyed_;
    }

private:
    int* destroyed_;
};

easylocal::generator<int> with_local(int* destroyed)
{
    const destruction_counter local{destroyed};
    for (int value = 0;; ++value)
        co_yield value;
}

} // namespace

int main()
{
    bool ok = true;

    std::vector<int> values;
    for (const int value : count_to(4))
        values.push_back(value);
    ok &=
        expect(values == std::vector<int>{10, 20, 30, 40}, "yields the values in order");

    int empty = 0;
    for ([[maybe_unused]] const int value : count_to(0))
        ++empty;
    ok &= expect(empty == 0, "an empty coroutine yields nothing");

    std::string seen;
    std::vector<std::string> texts;
    for (auto&& text : lvalue_twice(seen))
        texts.push_back(std::move(text));
    ok &= expect(
        texts.size() == 2 && texts[0] == texts[1]
            && seen == "a text longer than the small string buffer",
        "moving from a yielded lvalue leaves the coroutine's own value");

    int sum = 0;
    for (auto&& pointer : owned(4))
    {
        const std::unique_ptr<int> taken = std::move(pointer);
        sum += *taken;
    }
    ok &= expect(sum == 6, "a move-only value is yielded without a copy");

    int before_throw = 0;
    try
    {
        for ([[maybe_unused]] const int value : throw_after(3))
            ++before_throw;
        ok &= expect(false, "an exception of the coroutine reaches the consumer");
    }
    catch (const std::runtime_error& error)
    {
        ok &= expect(
            before_throw == 3 && std::string_view{error.what()} == "from the coroutine",
            "an exception of the coroutine reaches the consumer after its values");
    }

    try
    {
        auto moves = throw_after(0);
        static_cast<void>(moves.begin());
        ok &= expect(false, "an exception before the first value reaches begin()");
    }
    catch (const std::runtime_error&)
    {
    }

    int destroyed = 0;
    {
        auto values_with_local = with_local(&destroyed);
        int taken = 0;
        for (const int value : values_with_local)
        {
            if (value == 2)
                break;
            ++taken;
        }
        ok &= expect(taken == 2 && destroyed == 0, "a generator stops where it is left");
    }
    ok &=
        expect(destroyed == 1, "destroying an unfinished generator destroys its locals");

    return ok ? 0 : 1;
}
