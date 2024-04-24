//
//  utils.hh
//  easy-poc
//
//  Created by Luca Di Gaspero on 09/03/23.
//

#pragma once
#include <optional>

#ifndef EXPERIMENTAL_COROUTINES
#include <coroutine>  // std::suspend_never
#include <iterator>
#else
#include <experimental/coroutine>
#endif
#include <utility>   // std::forward, std::exchange
#include <concepts>

#ifdef EXPERIMENTAL_COROUTINES
namespace std {
  using experimental::suspend_never;
  using experimental::suspend_always;
  using experimental::coroutine_handle;
}
#endif

namespace easylocal {

  // coroutine handler for generators

  template<std::movable T>
  class Generator
  {
  public:
    struct promise_type
    {
      Generator<T> get_return_object()
      {
        return Generator{Handle::from_promise(*this)};
      }
      
      static std::suspend_always initial_suspend() noexcept
      {
        return {};
      }
      
      // co_yield
      std::suspend_always yield_value(T value) noexcept
      {
        current_value = std::move(value);
        return {};
      }
          
      // Disallow co_await in generator coroutines.
      void await_transform() = delete;
      
      [[noreturn]]
      static void unhandled_exception() { throw; }
      
      void return_void() noexcept {}
      
      static std::suspend_always final_suspend() noexcept
      {
        return {};
      }
      
      std::optional<T> current_value;
    };
    
    using Handle = std::coroutine_handle<promise_type>;
  
    explicit Generator(const Handle coroutine) :
    m_coroutine{coroutine}
    {}
    
    Generator() = default;
    ~Generator()
    {
      if (m_coroutine)
        m_coroutine.destroy();
    }
    
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;
    
    Generator(Generator&& other) noexcept : m_coroutine{other.m_coroutine}
    {
      other.m_coroutine = {};
    }
    
    Generator& operator=(Generator&& other) noexcept
    {
      if (this != &other)
      {
        if (m_coroutine)
          m_coroutine.destroy();
        m_coroutine = other.m_coroutine;
        other.m_coroutine = {};
      }
      return *this;
    }
    
    // Range-based for loop support.
    class Iter
    {
    public:
      explicit Iter(const Handle coroutine) : m_coroutine{coroutine}
      {}
      
      void operator++()
      {
        m_coroutine.resume();
      }
      
      const T& operator*() const
      {
        return *m_coroutine.promise().current_value;
      }
      
      bool operator==(std::default_sentinel_t) const
      {
        return !m_coroutine || m_coroutine.done();
      }
      
    private:
      Handle m_coroutine;
    };
    
    Iter begin()
    {
      if (m_coroutine)
        m_coroutine.resume();
      return Iter{m_coroutine};
    }
    
    std::default_sentinel_t end() { return {}; }
    
  private:
    Handle m_coroutine;
  };

  // tuple and variant utilities

  template <class Tuple, class F>
  constexpr decltype(auto) for_each(Tuple&& tuple, F&& f)
  {
    return [] <std::size_t... I>
    (Tuple&& tuple, F&& f, std::index_sequence<I...>) {
      (f(std::get<I>(tuple)), ...);
      return f;
    }(std::forward<Tuple>(tuple), std::forward<F>(f),
      std::make_index_sequence<std::tuple_size<std::remove_reference_t<Tuple>>::value>{});
  }

  template<typename Tuple, typename Action>
  Action perform(Tuple&& tuple, size_t index, Action action)
  {
    size_t currentIndex = 0;
    for_each(tuple, [&action, index, &currentIndex](auto&& value) {
      if (currentIndex == index)
      {
        action(std::forward<decltype(value)>(value));
      }
      ++currentIndex;
    });
    return action;
  }

  template<std::size_t N = 0, typename T, typename... Types>
  constexpr std::size_t variant_index() {
    if constexpr (N == sizeof...(Types)) {
        return N; // Return N (number of types) if T is not found.
    } else if constexpr (std::is_same_v<T, std::variant_alternative_t<N, std::variant<Types...>>>) {
        return N; // Found the type T at index N.
    } else {
        return variant_index<N + 1, T, Types...>(); // Recurse with N+1.
    }
  }
}
