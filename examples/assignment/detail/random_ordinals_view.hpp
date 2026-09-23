#pragma once

#include <cstddef>
#include <iterator>
#include <random>
#include <ranges>
#include <unordered_map>

namespace easylocal::mwe::assignment::detail
{

template<std::uniform_random_bit_generator RNG>
class random_ordinals_view
    : public std::ranges::view_interface<random_ordinals_view<RNG>>
{
public:
    random_ordinals_view() = default;

    random_ordinals_view(
        RNG& rng,
        const std::size_t population_size) noexcept
        : rng_{&rng},
          population_size_{population_size}
    {
    }

    class iterator
    {
    public:
        using iterator_concept = std::input_iterator_tag;
        using value_type = std::size_t;
        using difference_type = std::ptrdiff_t;

        iterator() = default;

        iterator(
            RNG& rng,
            const std::size_t population_size)
            : rng_{&rng},
              population_size_{population_size},
              remaining_{population_size}
        {
            if (population_size_ != 0)
            {
                generate_current();
            }
        }

        [[nodiscard]]
        auto operator*() const noexcept -> value_type
        {
            return current_;
        }

        auto operator++() -> iterator&
        {
            ++emitted_;

            if (emitted_ < population_size_)
            {
                generate_current();
            }

            return *this;
        }

        void operator++(int)
        {
            ++*this;
        }

        friend auto operator==(
            const iterator& current,
            std::default_sentinel_t) noexcept -> bool
        {
            return current.emitted_ >= current.population_size_;
        }

    private:
        [[nodiscard]]
        auto mapped_or_identity(const std::size_t ordinal) const -> std::size_t
        {
            if (const auto found = swaps_.find(ordinal); found != swaps_.end())
            {
                return found->second;
            }

            return ordinal;
        }

        void generate_current()
        {
            // Sparse Fisher-Yates over ordinal positions. Only sampled/swapped
            // positions consume storage; the move space itself is never
            // materialized.
            std::uniform_int_distribution<std::size_t> distribution{
                0,
                remaining_ - 1,
            };

            const auto drawn_position = distribution(*rng_);
            const auto last_position = remaining_ - 1;

            current_ = mapped_or_identity(drawn_position);
            const auto replacement_value = mapped_or_identity(last_position);

            swaps_[drawn_position] = replacement_value;
            swaps_.erase(last_position);
            --remaining_;
        }

        RNG* rng_{nullptr};
        std::size_t population_size_{0};
        std::size_t emitted_{0};
        std::size_t remaining_{0};
        std::size_t current_{0};
        std::unordered_map<std::size_t, std::size_t> swaps_;
    };

    [[nodiscard]]
    auto begin() -> iterator
    {
        return iterator{
            *rng_,
            population_size_,
        };
    }

    [[nodiscard]]
    auto end() const noexcept -> std::default_sentinel_t
    {
        return {};
    }

    [[nodiscard]]
    auto size() const noexcept -> std::size_t
    {
        return population_size_;
    }

private:
    RNG* rng_{nullptr};
    std::size_t population_size_{0};
};

} // namespace easylocal::mwe::assignment::detail
