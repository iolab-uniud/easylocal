

// begin --- easylocal --- 



// end --- easylocal --- 



// begin --- tabu-search.hh --- 

//
//  tabu-search.hh
//
//  Created by Luca Di Gaspero on 24/07/23.
//

#pragma once

// begin --- solution-manager.hh --- 

//
//  solution-manager.hh
//  poc
//
//  Created by Luca Di Gaspero on 09/03/23.
//

#pragma once

// begin --- concepts.hh --- 

//
//  concepts.hh
//  easy-poc
//
//  Created by Luca Di Gaspero on 09/03/23.
//

// TODO: split concepts in atomic ones, consider writing your struct for template specialization (i.e., a struct with a constexpr auto value = true / false) depending on the template parameters
// template <class T>
// struct OddDetectorImpl {
//   static constexpr auto value = false;
// };

// template <template <class...> class Tmpl, class... Args>
// requires (sizeof...(Args) % 2 == 1)
// struct OddDetectorImpl<Tmpl<Args>> {
//   static constexpr auto value = true;
// };
// template class<T>
// concept OddDetector = OddDetectorImpl<T>::value;

// TODO: use concepts with static_assert(Concept<Class>) for proper testing the framework

#pragma once

#include <concepts>
#include <type_traits>
#include <string>
#include <iostream>

// begin --- utils.hh --- 

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


// end --- utils.hh --- 



namespace easylocal {

  template <typename T>
  concept Number = std::is_arithmetic_v<T>;

  template <typename Input>
  concept InputT = std::is_constructible_v<Input, std::string> || std::is_constructible_v<Input, std::istream> || std::is_constructible_v<Input, int>; 

  template <typename Solution, typename Input>
  concept SolutionT = InputT<Input> && requires(Solution s) {
    //check that the solution class has a shared pointer to a const input object
  { s.in } -> std::same_as<std::shared_ptr<const Input>&>;
  };

  template <typename T>
  concept Printable = requires(std::ostream& os, const T& s) {
    // check that the class has a << operator for output
    { os << s };
  };

  template <typename Class, typename Input, typename Solution, typename T>
  concept match_basic_classes = InputT<Input> && 
    SolutionT<Solution, Input> && 
    Number<T>;

  template <typename Class>
  concept has_basic_typedefs = requires() {
    typename Class::Input;
    InputT<typename Class::Input>;
    typename Class::Solution;
    SolutionT<typename Class::Solution, typename Class::Input>;
    typename Class::T;
    Number<typename Class::T>;
  };

  template <typename Class, typename Input, typename Solution, typename T>
  concept match_basic_typedefs = match_basic_classes<Class, Input, Solution, T> && 
  requires() {
    typename Class::Input;
    typename Class::Solution;
    typename Class::T;
    std::same_as<typename Class::Input, Input>;
    std::same_as<typename Class::Solution, Solution>;
    std::same_as<typename Class::T, T>;
  };

  template <typename CostComponent, class Input, class Solution, typename T>
  concept CostComponentT = match_basic_classes<CostComponent, Input, Solution, T> && 
  requires(CostComponent cc, std::shared_ptr<const Solution> sol) {
    { cc.ComputeCost(sol) } -> std::same_as<T>;
  };

  template <typename DeltaCostComponent, class Input, class Solution, typename T, typename Move>
  concept DeltaCostComponentT = match_basic_classes<DeltaCostComponent, Input, Solution, T> && 
  requires(DeltaCostComponent dcc, std::shared_ptr<const Solution> sol, const Move& mv) {
    { dcc.ComputeDeltaCost(sol, mv) } -> std::same_as<T>;
      // TODO: check if still needed
//    { dcc.Components() } -> std::same_as<size_t>;
  };

  template <typename CostStructure, typename Input, typename Solution, typename T>
  concept CostStructureT = match_basic_typedefs<CostStructure, Input, Solution, T> && 
  requires(CostStructure s, std::shared_ptr<const Solution> sol, size_t i)
  {
    { s.ComputeCost(sol, i) } -> std::same_as<T>;
    { s.Components() } -> std::same_as<size_t>;
  };

  template <typename CostStructure>
  concept CostStructureTd = has_basic_typedefs<CostStructure> && 
  requires(CostStructure s, std::shared_ptr<const typename CostStructure::Solution> sol, size_t i)
  {
    { s.ComputeCost(sol, i) } -> std::same_as<typename CostStructure::T>;
    { s.Components() } -> std::same_as<size_t>;
  };

  template <class SolutionValue, class Input, class Solution, class T, class CostStructure>
  concept SolutionValueT = match_basic_typedefs<SolutionValue, Input, Solution, T> && CostStructureT<CostStructure, Input, Solution, T> && 
  requires(size_t i, SolutionValue sv) {
    { sv[i] } -> std::same_as<T>;
  };

  template<typename T, typename U>
  concept same_as_unqualified = std::same_as<std::remove_cvref_t<T>, std::remove_cvref_t<U>>;

  template <class SolutionManager>
  concept SolutionManagerT = has_basic_typedefs<SolutionManager> && 
  requires(std::shared_ptr<typename SolutionManager::Input> in, SolutionManager sm) {
    { sm.InitialSolution(in) } -> std::same_as<std::shared_ptr<typename SolutionManager::Solution>>;
    typename SolutionManager::CostStructure;
  };

  template <class NeighborhoodExplorer>
  concept NeighborhoodExplorerT = has_basic_typedefs<NeighborhoodExplorer> && 
requires(NeighborhoodExplorer ne, typename NeighborhoodExplorer::Solution& sol, std::shared_ptr<const typename NeighborhoodExplorer::Solution> cp_sol, std::shared_ptr<typename NeighborhoodExplorer::Solution> p_sol, typename NeighborhoodExplorer::Move mv) {
    typename NeighborhoodExplorer::SolutionManager;
    std::is_constructible_v<std::shared_ptr<const typename NeighborhoodExplorer::SolutionManager>>;
    typename NeighborhoodExplorer::Move;
    { ne.RandomMove(cp_sol) } -> std::same_as<typename NeighborhoodExplorer::Move>;
    { ne.MakeMove(p_sol, mv)};
    { ne.Neighborhood(cp_sol)} -> std::same_as<Generator<typename NeighborhoodExplorer::Move>>;
  };

  template <class NeighborhoodExplorer>
  concept has_inverse_move = 
requires(NeighborhoodExplorer ne, typename NeighborhoodExplorer::Solution& sol, typename NeighborhoodExplorer::Move mv1, typename NeighborhoodExplorer::Move mv2) {
    { ne.InverseMove(sol, mv1, mv2) } -> std::same_as<bool>;
  };
}


// end --- concepts.hh --- 



// begin --- cost-components.hh --- 

#pragma once
#include <vector>
#include <cassert>

namespace easylocal {

template <InputT Input, SolutionT<Input> Solution, Number T>
class CostComponent
{
public:
    /// Solution is passed as a const reference for performance reasons
    /// Indeed the solution is not acquired by the cost component, but it is used only to
    /// compute the cost (this allows to avoid shared_ptr increment/decrement count which
    /// requires atomic operations and thread syncrhonization)
    virtual T ComputeCost(std::shared_ptr<const Solution> s) const = 0;
    virtual ~CostComponent() = default;
};

// TODO: specify the right type for the cost component
template <InputT Input, SolutionT<Input> Solution, Number T, typename Move>
class DeltaCostComponent
{
public:
    /// Solution is passed as a const reference for performance reasons (see above in @CostComponent)
    virtual T ComputeDeltaCost(std::shared_ptr<const Solution> s, const Move& mv) const = 0;
    virtual ~DeltaCostComponent() = default;
};

template <InputT Input, SolutionT<Input> _Solution, Number _T, CostStructureTd _CostStructure, class NeighborhoodExplorer>
class MoveValue;

template <SolutionManagerT SolutionManager, class Move, class NE>
class NeighborhoodExplorer;

template <SolutionManagerT SolutionManager, class NE, class ...NHEs>
requires (NeighborhoodExplorerT<NHEs> && ...)
class UnionNeighborhoodExplorer;

template <InputT _Input, SolutionT<_Input> _Solution, Number _T>
class AggregatedCostStructure;

template <InputT _Input, SolutionT<_Input> _Solution, Number _T, class _CostStructure>
class SolutionValue : std::vector<std::pair<bool, _T>>
{
public:
    using Input = _Input ;
    using Solution = _Solution;
    using T = _T;
    using CostStructure = _CostStructure ;
    friend CostStructure;
    template <InputT I, SolutionT<I> S, Number T_, CostStructureTd CS, class NE> friend class MoveValue;
    template <SolutionManagerT SM, class Move, class NE> friend class NeighborhoodExplorer;
    template <SolutionManagerT SM, class NE, class ...NHEs> requires (NeighborhoodExplorerT<NHEs> && ...) friend class UnionNeighborhoodExplorer;
    
    std::shared_ptr<const Solution> GetSolution() const
    {
        return sol;
    }
    
    // TODO: this is a bit patchy, just to access a single scalar in case of aggregated costs due to its use in TS, review in another life
    template <typename = std::enable_if<std::same_as<CostStructure, class AggregatedCostStructure<Input, Solution, T>>>>
    T AggregatedCost() const
    {
        return cs->ComputeAggregatedCost(*this);
    }
    
    std::vector<T> GetValues() const
    {
        std::vector<T> tmp;
        tmp.reserve(this->size());
        for (size_t i = 0; i < this->size(); ++i)
            tmp.push_back((*this)[i]);
        return tmp;
    }
    
    bool CheckValues() const
    {
        for (size_t i = 0; i < this->size(); ++i)
            if (cs->ComputeCost(sol, i) != (*this)[i])
                return false;
        return true;
    }
    
    T operator[](size_t i) const
    {
        //const std::lock_guard<std::mutex> lock(compute);
        auto& val = const_cast<std::pair<bool, T>&>(std::vector<std::pair<bool, T>>::operator[](i));
        if (!val.first)
        {
            val.second = cs->ComputeCost(sol, i);
            val.first = true;
        }
        return val.second;
    }
    
    using std::vector<std::pair<bool, T>>::size;
    
    template <SolutionValueT<Input, Solution, T, CostStructure> SV>
    auto operator<=>(const SV& other) const
    {
        return cs->spaceship(*this, other);
    }
    
    template <SolutionValueT<Input, Solution, T, CostStructure> SV>
    auto operator<=>(const std::shared_ptr<SV> other) const
    {
        return cs->spaceship(*this, *other);
    }
    
    template <SolutionValueT<Input, Solution, T, CostStructure> SV>
    auto operator==(const SV& other) const
    {
        return cs->equality(*this, other);
    }
    
    template <NeighborhoodExplorerT NeighborhoodExplorer>
    SolutionValue(const MoveValue<Input, Solution, T, _CostStructure, NeighborhoodExplorer>& m) : cs(m.cs), sol(m.GetSolution())
    {
        auto values = m.GetValues();
        this->reserve(m.size());
        for (size_t i = 0; i < values.size(); ++i)
            (*this).push_back({ true, values[i] });
    }
    
    SolutionValue(const SolutionValue<Input, Solution, T, _CostStructure>& s) : cs(s.cs), sol(s.sol), std::vector<std::pair<bool, T>>(s)
    {}
    
protected:
    SolutionValue(std::shared_ptr<const CostStructure> cs, std::shared_ptr<const Solution> sol, size_t components) : cs(cs), sol(sol), std::vector<std::pair<bool, T>>(components, { false, 0 })
    {
        assert(cs && sol);
    }
    using std::vector<std::pair<bool, T>>::begin;
    using std::vector<std::pair<bool, T>>::end;
    using std::vector<std::pair<bool, T>>::at;
    std::shared_ptr<const CostStructure> cs;
    std::shared_ptr<const Solution> sol;
};

template <InputT _Input, SolutionT<_Input> _Solution, Number _T, CostStructureTd _CostStructure, class _NeighborhoodExplorer>
class MoveValue : std::vector<std::pair<bool, _T>>
{
public:
    using Input = _Input;
    using Solution = _Solution ;
    using T = _T;
    using CostStructure = _CostStructure;
protected:
    using Move = typename _NeighborhoodExplorer::Move;
    using SolutionValue = SolutionValue<Input, _Solution, _T, _CostStructure>;
    friend SolutionValue;
    using NeighborhoodExplorer = typename _NeighborhoodExplorer::ThisClass;
    friend NeighborhoodExplorer;
public:
    
    Move GetMove() const
    {
        return mv;
    }
    
    // TODO: this is a bit patchy, just to access a single scalar in case of aggregated costs due to its use in TS, review in another life
    template <typename = std::enable_if<std::same_as<CostStructure, class AggregatedCostStructure<Input, Solution, T>>>>
    T AggregatedCost() const
    {
        return this->GetSolutionValue().AggregatedCost();
    }
    
    std::vector<T> GetValues() const
    {
        std::vector<T> tmp;
        tmp.reserve(this->size());
        for (size_t i = 0; i < this->size(); ++i)
            tmp.push_back((*this)[i]);
        return tmp;
    }
    
    T operator[](size_t i) const
    {
        auto& val = const_cast<std::pair<bool, T>&>(std::vector<std::pair<bool, T>>::operator[](i));
        if (!val.first)
        {
            // the value has to be computed
            if (ne->HasDeltaCostComponent(i, mv))
            {
                val.second = old_sv[i] + ne->ComputeDeltaCost(old_sv.GetSolution(), mv, i);
                val.first = true;
            }
            else
            {
                if (!new_sol)
                {
                    // make a copy of the solution
                    new_sol = std::make_shared<Solution>(*(old_sv.GetSolution()));
                    // apply the move
                    ne->MakeMove(new_sol, mv);
                }
                // compute the new cost directly from solution
                val.second = cs->ComputeCost(new_sol, i);
                val.first = true;
            }
        }
        return val.second;
    }
    
    template <SolutionValueT<Input, Solution, T, CostStructure> SV>
    auto operator<=>(const SV& other) const
    {
        return cs->spaceship(*this, other);
    }
    
    template <SolutionValueT<Input, Solution, T, CostStructure> SV2>
    auto operator==(const SV2& other) const
    {
        return cs->equality(*this, other);
    }
    
    std::shared_ptr<const Solution> GetSolution() const
    {
        // the new solution has not been determined yet
        if (!new_sol)
        {
            new_sol = std::make_shared<Solution>(*(old_sv.GetSolution())); // make a copy of the solution
            ne->MakeMove(new_sol, mv);
        }
        return new_sol;
    }
    
    SolutionValue GetSolutionValue() const
    {
        return cs->CreateSolutionValue(this->GetSolution());
    }
    
    using std::vector<std::pair<bool, T>>::size;
    
    MoveValue(const MoveValue<Input, Solution, T, _CostStructure, NeighborhoodExplorer>& m) : cs(m.cs), ne(m.ne), new_sol(m.new_sol), old_sv(m.old_sv), std::vector<std::pair<bool, T>>(m)
    {}
    
protected:
    MoveValue(std::shared_ptr<const _NeighborhoodExplorer> ne, const SolutionValue& sv, const Move& mv, size_t size) : mv(mv), old_sv(sv), cs(sv.cs), ne(ne), std::vector<std::pair<bool, T>>(size, { false, 0 })
    {
        assert(sv.cs && ne);
    }
    
    std::shared_ptr<const CostStructure> cs;
    std::shared_ptr<const _NeighborhoodExplorer> ne;
    Move mv;
    SolutionValue old_sv;
    mutable std::shared_ptr<Solution> new_sol;
};

template <InputT _Input, SolutionT<_Input> _Solution, Number _T>
class AggregatedCostStructure : public std::enable_shared_from_this<AggregatedCostStructure<_Input, _Solution, _T>>
{
public:
    using Input = _Input;
    using Solution = _Solution;
    using T = _T;
    friend class SolutionValue<Input, Solution, T, AggregatedCostStructure<Input, Solution, T>>;
    using SolutionValue = SolutionValue<Input, Solution, T, AggregatedCostStructure>;
protected:
    using SelfClass = AggregatedCostStructure<Input, Solution, T>;
    
public:
    template <CostComponentT<Input, Solution, T> CostComponent>
    void AddCostComponent(std::shared_ptr<CostComponent> cc, bool hard, double weight = 1.0)
    {
        // make a copy of the cost component
        cost_components.emplace_back(std::make_unique<CostComponent>(*cc));
        hard_components.emplace_back(hard);
        weight_components.emplace_back(weight);
    }
    
    SolutionValue CreateSolutionValue(std::shared_ptr<const Solution> sol) const
    {
        return { this->shared_from_this(), sol, cost_components.size() };
    }
    
    T ComputeCost(std::shared_ptr<const Solution> sol, size_t i) const
    {
        return this->cost_components[i]->ComputeCost(sol);
    }
    
    template <SolutionValueT<Input, Solution, T, SelfClass> SV1, SolutionValueT<Input, Solution, T, SelfClass> SV2>
    bool equality(const SV1& sc1, const SV2& sc2) const
    {
        assert(this->cost_components.size() == sc1.size() && this->cost_components.size() == sc2.size());
        T total_cost_1 = this->ComputeAggregatedCost(sc1), total_cost_2 = this->ComputeAggregatedCost(sc2);
        // TODO: consider floating point approximated equality at some point, use SFINAE
        return (total_cost_1 == total_cost_2);
    }
    
    template <SolutionValueT<Input, Solution, T, SelfClass> SV1, SolutionValueT<Input, Solution, T, SelfClass> SV2>
    std::strong_ordering spaceship(const SV1& sc1, const SV2& sc2) const
    {
        assert(this->cost_components.size() == sc1.size() && this->cost_components.size() == sc2.size());
        T total_cost_1 = this->ComputeAggregatedCost(sc1), total_cost_2 = this->ComputeAggregatedCost(sc2);
        // TODO: consider floating point approximated equality at some point, use SFINAE
        if (total_cost_1 < total_cost_2)
            return std::strong_ordering::less;
        else if (total_cost_1 == total_cost_2)
            return std::strong_ordering::equal;
        else
            return std::strong_ordering::greater;
    }
    
    size_t Components() const
    {
        return this->cost_components.size();
    }
    
protected:
    template <SolutionValueT<Input, Solution, T, SelfClass> SV>
    T ComputeAggregatedCost(const SV& sv) const
    {
        T cost_H = 0, cost_S = 0;
        for (size_t i = 0; i < this->cost_components.size(); ++i)
        {
            if (this->hard_components[i])
                cost_H += this->weight_components[i] * sv[i];
            else
                cost_S += this->weight_components[i] * sv[i];
        }
        return this->HARD_WEIGHT * cost_H + cost_S;
    }
    
    std::vector<std::unique_ptr<CostComponent<Input, Solution, T>>> cost_components;
    std::vector<bool> hard_components;
    std::vector<double> weight_components;
    T HARD_WEIGHT = 1000;
};

template <InputT _Input, SolutionT<_Input> _Solution, Number _T>
class MultiObjectiveCostStructure : public std::enable_shared_from_this<MultiObjectiveCostStructure<_Input, _Solution, _T>>
{
public:
    using Input = _Input;
    using Solution = _Solution;
    using T = _T;
    friend class SolutionValue<Input, Solution, T, MultiObjectiveCostStructure<Input, Solution, T>>;
    using SolutionValue = SolutionValue<Input, Solution, T, MultiObjectiveCostStructure>;
    using SelfClass = MultiObjectiveCostStructure<_Input, _Solution, _T>;
    
    template <CostComponentT<Input, Solution, T> CostComponent>
    void AddCostComponent(std::shared_ptr<CostComponent> cc)
    {
        // make a copy of the cost component
        cost_components.emplace_back(std::make_unique<CostComponent>(*cc));
    }
    
    SolutionValue CreateSolutionValue(std::shared_ptr<const Solution> sol) const
    {
        return { this->shared_from_this(), sol, cost_components.size() };
    }
    
    T ComputeCost(std::shared_ptr<const Solution> sol, size_t i) const
    {
        return this->cost_components[i]->ComputeCost(sol);
    }
    
    template <SolutionValueT<Input, Solution, T, SelfClass> SV1, SolutionValueT<Input, Solution, T, SelfClass> SV2>
    bool equality(const SV1& sc1, const SV2& sc2) const
    {
        assert(this->cost_components.size() == sc1.size() && this->cost_components.size() == sc2.size());
        
        // TODO: consider floating point approximated equality at some point, use SFINAE
        for (size_t i = 0; i < this->cost_components.size(); ++i)
        {
            if (sc1[i] != sc2[i])
                return false;
        }
        return true;
    }
    
    template <SolutionValueT<Input, Solution, T, SelfClass> SV1, SolutionValueT<Input, Solution, T, SelfClass> SV2>
    std::partial_ordering spaceship(const SV1& sc1, const SV2& sc2) const
    {
        assert(this->cost_components.size() == sc1.size() && this->cost_components.size() == sc2.size());
        size_t sc1_less_than_sc2 = 0, sc1_greater_than_sc2 = 0;
        for (size_t i = 0; i < this->cost_components.size(); ++i)
        {
            if (sc1[i] < sc2[i])
            {
                if (sc1_greater_than_sc2 > 0)
                    return std::partial_ordering::unordered;
                sc1_less_than_sc2++;
            }
            else if (sc1[i] > sc2[i])
            {
                sc1_greater_than_sc2++;
                if (sc1_less_than_sc2 > 0)
                    return std::partial_ordering::unordered;
            }
        }
        if (sc1_less_than_sc2 > 0 && sc1_greater_than_sc2 == 0)
            return std::partial_ordering::less;
        if (sc1_greater_than_sc2 > 0 && sc1_less_than_sc2 == 0)
            return std::partial_ordering::greater;
        if (sc1_less_than_sc2 == 0 && sc1_greater_than_sc2 == 0)
            return std::partial_ordering::equivalent;
        return std::partial_ordering::unordered;
    }
    
    size_t Components() const
    {
        return this->cost_components.size();
    }
    
protected:
    std::vector<std::unique_ptr<CostComponent<Input, Solution, T>>> cost_components;
};
}


// end --- cost-components.hh --- 



namespace easylocal {
  // TODO: helper to transform a std::function (e.g., a lambda for random solution) in a SolutionManager
  template <InputT _Input, SolutionT<_Input> _Solution, Number _T, class _CostStructure>
  requires CostStructureT<_CostStructure, _Input, _Solution, _T>
  class SolutionManager : public _CostStructure
  {
  public:
    using Input = _Input;
    using Solution = _Solution;
    using T = _T;
    using CostStructure = _CostStructure;
    // the method InitialSolution(std::shared_ptr<Input> in) const should be defined in the actual Solution Manager subclass
  };
}


// end --- solution-manager.hh --- 



// begin --- components.hh --- 

#pragma once
#include <list>
#include <vector>
#include <map>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

// FIXME: -1 and size_t are archenemies!

// TODO: OR and AND of different components?
// TODO: divide components by type... (e.g., sub-directory with components and each type of component in a file)

namespace easylocal {

// TODO: currently it cannot check because idle_iteration is meant to be protected
template <class Runner>
concept RunnerIdleIterT = has_basic_typedefs<Runner>; /* &&
                                                       requires(Runner r) {
                                                       { r.idle_iteration } -> std::same_as<size_t&>;
                                                       }; */

class Parametrized
{
public:
    virtual void add_parameter(po::options_description& opt)
    {}
    virtual void print_parameters()
    {}
};


// TODO: give a more meaningful name
template <class Runner>
class FullNeighborhoodGenerator : public Parametrized
{
    using MoveValue = typename Runner::MoveValue;
public:
    easylocal::Generator<std::shared_ptr<MoveValue>> generate_moves(Runner* r)
    {
        for (auto mv : r->ne->Neighborhood(r->current_solution_value->GetSolution()))
        {
            co_yield std::make_shared<MoveValue>(r->ne->CreateMoveValue(*(r->current_solution_value), mv));
        }
    }
    virtual void initialize()
    {}
protected:
};

// TODO: give a more meaningful name
template <class Runner>
class EliteCandidateGenerator : public Parametrized
{
    using MoveValue = typename Runner::MoveValue;
    using Move = typename Runner::Move;
    using T = typename Runner::T;
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("k", po::value<size_t>(&k), "Size of the elite candidate list.");
        opt.add_options()
            ("theta", po::value<double>(&theta), "Size of the threshold (must be equal or greater than 1.0).");
    }
    void print_parameters() override
    {
        // spdlog::info("EliteCandidateGenerator - parameter k: {}", k);
        // spdlog::info("EliteCandidateGenerator - parameter theta: {}", theta);
    }
    void initialize()
    {
        assert(theta >= 1.0);
    }
    easylocal::Generator<std::shared_ptr<MoveValue>> generate_moves(Runner* r)
    {
        size_t best_index;
        threshold = (r->best_solution_value)->AggregatedCost() * theta;
        if (elite_candidates.size() == 0)
            build_elite_candidate_list(r);
        // if the candidate list is not empty
        assert(elite_candidates.size() > 0);
        best_index = search_best_elite_candidate_list(r);
        MoveValue best_move_value = r->ne->CreateMoveValue(*(r->current_solution_value), elite_candidates[best_index].GetMove());
        
        if (best_move_value.AggregatedCost() <= threshold)
        {
            co_yield std::make_shared<MoveValue>(best_move_value);
            elite_candidates.erase(elite_candidates.begin() + best_index);
        }
        else
        {
            build_elite_candidate_list(r);
            best_index = search_best_elite_candidate_list(r);
            MoveValue best_move_value = r->ne->CreateMoveValue(*(r->current_solution_value), elite_candidates[best_index].GetMove());
            if (best_move_value.AggregatedCost() > threshold)
                co_return;
            co_yield std::make_shared<MoveValue>(best_move_value);
            elite_candidates.erase(elite_candidates.begin() + best_index);
        }
    }
protected:
    void build_elite_candidate_list(Runner* r)
    {
        // clear the candidate list,
        elite_candidates.clear();
        // worse_move_index
        size_t worse_move_index = 0;
        // worse_move
        MoveValue worse_move_value = r->ne->CreateMoveValue(*(r->current_solution_value), r->ne->RandomMove(r->current_solution_value->GetSolution()));
        bool initialized = false;
        // for mv in neighborhood
        for (auto mv : r->ne->Neighborhood(r->current_solution_value->GetSolution()))
        {
            MoveValue current_move_value = r->ne->CreateMoveValue(*(r->current_solution_value), mv);
            // if candidate list size < k
            if (elite_candidates.size() < k)
            {
                if(!initialized)
                {
                    initialized = true;
                    worse_move_value = r->ne->CreateMoveValue(*(r->current_solution_value), mv);
                }
                // pushback
                elite_candidates.push_back(current_move_value);
                if (worse_move_value.AggregatedCost() < current_move_value.AggregatedCost())
                {
                    worse_move_value = current_move_value;
                    worse_move_index = elite_candidates.size() - 1; // - 1 because I already updated the candidate list
                }
            }
            // else
            else
            {
                // if better than worse, then put this in that place
                if(current_move_value.AggregatedCost() < worse_move_value.AggregatedCost())
                {
                    elite_candidates[worse_move_index] = current_move_value;
                    // find the new worse
                    worse_move_value = elite_candidates[0];
                    worse_move_index = 0;
                    for(size_t i = 1; i < elite_candidates.size(); ++i)
                    {
                        if (worse_move_value.AggregatedCost() < elite_candidates[i].AggregatedCost())
                        {
                            worse_move_index = i;
                            worse_move_value = elite_candidates[i];
                        }
                    }
                }
            }
        }
    }
    
    size_t search_best_elite_candidate_list(Runner* r)
    {
        size_t best_index = 0;
        MoveValue best_move_value = r->ne->CreateMoveValue(*(r->current_solution_value), elite_candidates[0].GetMove());
        for (size_t i = 1; i < elite_candidates.size(); ++i)
        {
            MoveValue current_move_value = r->ne->CreateMoveValue(*(r->current_solution_value), elite_candidates[i].GetMove());
            if (current_move_value.AggregatedCost() < best_move_value.AggregatedCost())
            {
                best_move_value = current_move_value;
                best_index = i;
            }
        }
        return best_index;
    }
    size_t k;
    std::vector<MoveValue> elite_candidates;
    double theta;
    T threshold;
};

template <RunnerIdleIterT Runner>
class IdleIterationsTermination : public Parametrized
{
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("max-idle-iteration", po::value<size_t>(&max_idle_iterations), "Max number of idle iterations.");
    }
    void print_parameters() override
    {
        // spdlog::info("IdleIterationsTermination - parameter max_idle_iterations: {}", max_idle_iterations);
    }
    bool terminate(Runner* r)
    {
        return r->idle_iteration > max_idle_iterations;
    }
protected:
    size_t max_idle_iterations;
};

template <RunnerIdleIterT Runner>
class TotalIterationsTermination : public Parametrized
{
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("max-total-iterations", po::value<size_t>(&max_iterations), "Maximum number of iterations.");
    }
    void print_parameters() override
    {
        // spdlog::info("TotalIterationsTermination - parameter max_iterations: {}", max_iterations);
    }
    bool terminate(Runner* r)
    {
        return r->iteration > max_iterations;
    }
protected:
    size_t max_iterations;
};

// TODO: define the proper concept for TabuList
template <class Runner>
class FixedLengthTabuList : public Parametrized
{
    using Move = typename Runner::Move;
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("max-length", po::value<size_t>(&max_length), "Maximum length of the tabu list.");
    }
    void print_parameters() override
    {
        // spdlog::info("FixedLengthTabuList - parameter max_length: {}", max_length);
        // spdlog::info("FixedLengthTabuList - parameter current: {}", current);
    }
    void initialize(Runner* r)
    {
        current = 0;
    }
    bool is_tabu(Runner* r)
    {
        const auto& current_move = r->current_move_value->GetMove();
        const auto& current_solution = r->current_move_value->GetSolution();
        for (const auto& tl_move : tabu_moves)
        {
            if (r->ne->Inverse(current_solution, current_move, tl_move))
            {
#if !defined(NDEBUG)
                std::ostringstream oss;
                // oss << tl_move;
                //spdlog::debug("FixedLengthTabuList - move {} is tabu", oss.str());
#endif
                return true;
            }
        }
#if !defined(NDEBUG)
        spdlog::debug("FixedLengthTabuList - move is NOT tabu");
#endif
        return false;
    }
    void update(Runner* r)
    {
        if (tabu_moves.size() < max_length)
        {
            tabu_moves.push_back(r->best_move_value->GetMove());
        }
        else
        {
            tabu_moves[current] = r->best_move_value->GetMove();
            current = (current + 1) % max_length;
        }
    }
    Move least_tabu(Runner* r)
    {
#if !defined(NDEBUG)
        spdlog::debug("FixedLengthTabuList - retrieve least tabu move");
#endif
        return tabu_moves[current];
    }
protected:
    std::vector<Move> tabu_moves;
    size_t current, max_length;
};

template <class Runner>
class FixedLengthObjectiveBasedTabuList : public Parametrized
{
    using Move = typename Runner::Move;
    using T = typename Runner::T;
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("max-length", po::value<size_t>(&max_length), "Maximum length of the tabu list.");
    }
    void print_parameters() override
    {
        // spdlog::info("FixedLengthObjectiveBasedTabuList - parameter max_length: {}", max_length);
        // spdlog::info("FixedLengthObjectiveBasedTabuList - parameter current: {}", current);
    }
    void initialize(Runner* r)
    {
        current = 0;
    }
    bool is_tabu(Runner* r)
    {
        const T& current_move = r->current_move_value->AggregatedCost();
        // const auto& current_solution = r->current_move_value->GetSolution();
#if !defined(NDEBUG)
        spdlog::debug("FixedLengthObjectiveBasedTabuList - move cost is {}", current_move);
#endif
        for (const auto& tl_move : tabu_moves)
        {
            if (tl_move == current_move)
            {
#if !defined(NDEBUG)
                spdlog::debug("FixedLengthObjectiveBasedTabuList - move {} is tabu", tl_move);
#endif
                return true;
            }
        }
#if !defined(NDEBUG)
        spdlog::debug("FixedLengthObjectiveBasedTabuList - move is NOT tabu");
#endif
        return false;
    }
    void update(Runner* r)
    {
        if (tabu_moves.size() < max_length)
        {
            tabu_moves.push_back(r->best_move_value->AggregatedCost());
        }
        else
        {
            tabu_moves[current] = r->best_move_value->AggregatedCost();
            current = (current + 1) % max_length;
        }
    }
    Move least_tabu(Runner* r)
    {
#if !defined(NDEBUG)
        spdlog::debug("FixedLengthObjectiveBasedTabuList - retrieve least tabu move");
#endif
        return r->ne->RandomMove(r->current_solution_value->GetSolution());
    }
protected:
    std::vector<T> tabu_moves;
    size_t current, max_length;
};

template <class Runner>
class LimDynamicTabuList : public Parametrized
{
    using Move = typename Runner::Move;
    using T = typename Runner::T;
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("max-length", po::value<size_t>(&max_length), "Maximum length of the tabu list.")
            ("min-length", po::value<size_t>(&min_length), "Minimum length of the tabu list.")
            ("iteration-threshold", po::value<size_t>(&iteration_threshold), "Threshold for iterations");
    }
    void print_parameters() override
    {
        // spdlog::info("LimDynamicTabuList - parameter max-length: {}", max_length);
        // spdlog::info("LimDynamicTabuList - parameter min-length: {}", min_length);
        // spdlog::info("LimDynamicTabuList - parameter - iteration_threshold: {}", iteration_threshold);
        // spdlog::info("LimDynamicTabuList - parameter current: {}", current);
    }
    void initialize(Runner* r)
    {
        current = 0;
        assert(min_length < max_length);
    }
    bool is_tabu(Runner* r)
    {
        const auto& current_move = r->current_move_value->GetMove();
        const auto& current_solution = r->current_move_value->GetSolution();
        for (const auto& tl_move : tabu_moves)
        {
            if (r->ne->Inverse(current_solution, current_move, tl_move))
            {
                return true;
            }
        }
        return false;
    }
    void update(Runner* r)
    {
        if (tabu_moves.size() < min_length || r->idle_iteration >= iteration_threshold)
        {
            tabu_moves.push_back(r->best_move_value->GetMove());
        }
        else
        {
            tabu_moves[current] = r->best_move_value->GetMove();
            current = (current + 1) % tabu_moves.size();
        }
        
        if (r->idle_iteration == 0 || tabu_moves.size() >= max_length)
        {
            size_t cnt;
            if (tabu_moves.size() > min_length)
                cnt = tabu_moves.size() - min_length;
            else
                cnt = min_length - tabu_moves.size();
            
            while (cnt > 0)
            {
                tabu_moves.erase(tabu_moves.begin() + current);
                current = (current + 1) % tabu_moves.size();
                cnt--;
            }
        }
    }
    Move least_tabu(Runner* r)
    {
#if !defined(NDEBUG)
        spdlog::debug("LimDynamicTabuList - retrieve least tabu move");
#endif
        return tabu_moves[current];
    }
protected:
    std::vector<Move> tabu_moves;
    size_t current, max_length, min_length, iteration_threshold;
};

template <class Runner>
class TaillardTabuList : public Parametrized
{
    using Move = typename Runner::Move;
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("max-length-it", po::value<size_t>(&max_length_it), "Insert description")
            ("tabu-sizes", po::value<std::vector<size_t>>(&tabu_sizes), "Insert description");
    }
    void print_parameters() override
    {
        // spdlog::info("TaillardTabuList - parameter max_length_it: {}", max_length_it);
        // spdlog::info("TaillardTabuList - parameter tabu_sizes: {}", spdlog::fmt_lib::join(tabu_sizes, ", "));
        // spdlog::info("TaillardTabuList - parameter current: {}", current);
        // spdlog::info("TaillardTabuList - parameter current_length_index: {}", current_length_index);
    }
    void initialize(Runner* r)
    {
        current = 0;
        current_length_index = 0;
    }
    bool is_tabu(Runner* r)
    {
        const auto& current_move = r->current_move_value->GetMove();
        const auto& current_solution = r->current_move_value->GetSolution();
        for (const auto& tl_move : tabu_moves)
        {
            if (r->ne->Inverse(current_solution, current_move, tl_move))
            {
                return true;
            }
        }
        return false;
    }
    void update(Runner* r)
    {
        current_length = tabu_sizes[current_length_index];
        
        if (tabu_moves.size() < current_length)
        {
            tabu_moves.push_back(r->best_move_value->GetMove());
#if !defined(NDEBUG)
            spdlog::debug("TaillardTabuList - Adding move to the list, because list is to small ({}// {})", tabu_moves.size() - 1, current_length);
#endif
        }
        else
        {
            tabu_moves[current] = r->best_move_value->GetMove();
#if !defined(NDEBUG)
            spdlog::debug("TaillardTabuList - Changing index {} in tabu list of size {}", current, current_length);
#endif
            current = (current + 1) % current_length;
        }
        
        if (length_it == max_length_it)
        {
            current_length_index = (current_length_index + 1) % tabu_sizes.size();
            length_it = 0;
        }
        else
        {
            length_it = length_it + 1;
        }
    }
    Move least_tabu(Runner* r)
    {
#if !defined(NDEBUG)
        spdlog::debug("TaillardTabuList - retrieve least tabu move");
#endif
        size_t i = (current + 1) % current_length;
        return tabu_moves[i];
    }
protected:
    std::vector<Move> tabu_moves;
    std::vector<size_t> tabu_sizes;
    size_t current, current_length_index; // indexes
    size_t current_length; // this is the current size to consider of tabu_moves
    size_t length_it, max_length_it; // this is the number of iterations that the current_length has been used
};

template <class Runner>
class GendrauTabuList : public Parametrized
{
    using Move = typename Runner::Move;
public:
    //FIXME: rng 10, 10 should be something more principled as the seed for e.g.
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("min-iteration-tl", po::value<size_t>(&min_iteration), "Minimum number of iterations you can have.");
        opt.add_options()
            ("max-iteration-tl", po::value<size_t>(&max_iteration), "Maximum number of iterations you can have.");
    }
    void print_parameters() override
    {
        // spdlog::info("GendrauTabuList - parameter min_iteration: {}", min_iteration);
        // spdlog::info("GendrauTabuList - parameter max_iteration: {}", max_iteration);
        // // spdlog::info("GendrauTabuList - parameter random seed: {}", rng.seed());
    }
    void initialize(Runner* r)
    {
        rng.seed(r->random_seed);
    }
    bool is_tabu(Runner* r)
    {
        const auto& current_move = r->current_move_value->GetMove();
        const auto& current_solution = r->current_move_value->GetSolution();
        for (const auto& tl_move : tabu_moves)
        {
            if (r->ne->Inverse(current_solution, current_move, tl_move.first))
            {
#if !defined(NDEBUG)
                spdlog::debug("GendrauTabuList - move is tabu");
#endif
                return true;
            }
        }
        return false;
    }
    void update(Runner* r)
    {
#if !defined(NDEBUG)
        spdlog::debug("GendrauTabuList - update tabu list");
#endif
        // insert in the tabu list best_move_value, the iteration will be current_iteration + number of iterations drown at random
        std::uniform_int_distribution<size_t> dist_iterations(min_iteration, max_iteration);
        size_t delta_iteration = dist_iterations(rng);
        size_t removal_iteration = delta_iteration + r->iteration;
        tabu_moves.push_back(std::make_pair(r->best_move_value->GetMove(), removal_iteration));
        // scan tabu_moves, if some moves have tl_move.second == current iteration, remove it
        for (size_t i = 0; i < tabu_moves.size(); ++i)
        {
            if (tabu_moves[i].second == r->iteration)
            {
                tabu_moves.erase(tabu_moves.begin() + i);
            }
        }
    }
    Move least_tabu(Runner* r)
    {
#if !defined(NDEBUG)
        spdlog::debug("GendrauTabuList - retrieve least tabu move");
#endif
        Move least_tabu = tabu_moves[0].first;
        size_t least_it = tabu_moves[0].second;
        for (size_t i = 1; i < tabu_moves.size(); ++i)
        {
            if (tabu_moves[i].second < least_it)
            {
                least_tabu = tabu_moves[i].first;
                least_it = tabu_moves[i].second;
            }
        }
        return least_tabu;
    }
protected:
    std::vector<std::pair<Move, size_t>> tabu_moves; // tabu_moves contain a pair, that is the move and the iteration at which the move will be removed
    size_t min_iteration, max_iteration;
    mutable std::mt19937_64 rng;
};

template <class Runner>
class ReactiveTabuList : public Parametrized
{
    using Move = typename Runner::Move;
    using Solution = typename Runner::Solution;
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("cycle-max", po::value<size_t>(&cycle_max), "Insert description");
    }
    void print_parameters() override
    {
        // spdlog::info("ReactiveTabuList - parameter cycle_max: {}", cycle_max);
        // spdlog::info("ReactiveTabuList - parameter cycleMoveAve: {}", cycleMoveAve);
        // spdlog::info("ReactiveTabuList - parameter last_update_iteration: {}", last_update_iteration);
    }
    void initialize(Runner* r)
    {
        cycleMoveAve = 0;
        last_update_iteration = 0;
    }

    bool is_tabu(Runner* r)
    {
        const auto& current_move = r->current_move_value->GetMove();
        const auto& current_solution = r->current_move_value->GetSolution();
        for (const auto& tl_move : tabu_moves)
        {
            if (r->ne->Inverse(current_solution, current_move, tl_move))
            {
#if !defined(NDEBUG)
                std::ostringstream oss;
                oss << tl_move;
                spdlog::debug("ReactiveTabuList - move {} is tabu", oss.str());
#endif
                return true;
            }
        }
#if !defined(NDEBUG)
        spdlog::debug("ReactiveTabuList - move is NOT tabu");
#endif
        return false;
    }
    void update(Runner* r)
    {
        // see: https://github.com/reichlin/Kitchen2000/blob/master/Reactive%20tabu%20Search/RTS.java#L397
        const auto& current_move = r->best_move_value->GetMove();
        const auto& current_solution = r->current_move_value->GetSolution();
        const size_t& current_iteration = r->iteration;
        
        // find whether the current solution was already encountered, and set last_met_iteration accordingly
        if (already_met_solution(current_solution, current_iteration))
        {
#if !defined(NDEBUG)
            spdlog::debug("ReactiveTabuList - solution met");
#endif
            // update cycleMoveAve
            cycleMoveAve = (size_t) ((cycleMoveAve * current_iteration + last_met_iteration) / current_iteration);
            // update lastupdate
            last_update_iteration = current_iteration;
        }
        // else if (iterations - last_update) > cycloMoveAve
        else if(current_iteration - last_update_iteration > cycleMoveAve)
        {
            // if tabu list is not empty
            if (tabu_moves.size() > 0)
                // remove the first element of the tabu list
                tabu_moves.erase(tabu_moves.begin());
        }
        // if history contain the maximum (cycle_max)
        if (history.size() >= cycle_max)
            // TODO: safely remove cycle_max - history.size() + 1 solutions
            // remove the first element of the tabu list
            history.erase(history.begin());
        
        // enqueue in tabu_moves and history the move and the current solution rispectively
        history.push_back(current_solution);
        tabu_moves.push_back(current_move);
    }
    Move least_tabu(Runner* r)
    {
#if !defined(NDEBUG)
        spdlog::debug("ReactiveTabuList - retrieve least tabu move");
#endif
        // the least tabu move is the oldest one
        return tabu_moves[0];
    }
protected:
    bool already_met_solution(std::shared_ptr<const Solution> solution, size_t current_iteration)
    {
        if (history.size() == 0)
            return false;
        // look for it
        for (size_t i = history.size() - 1; i >= 0; --i)
        {
            if (history[i] == solution)
            {
                last_met_iteration = current_iteration + history.size() - i;
                return true;
            }
            
        }
        return false;
    }
    std::vector<Move> tabu_moves;
    std::vector<std::shared_ptr<const Solution>> history;
    size_t cycle_max, cycleMoveAve;
    size_t last_met_iteration, last_update_iteration;
};

template <class Runner>
class TransitionMeasureTabuList : public Parametrized
{
    using Move = typename Runner::Move;
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("frequency", po::value<double>(&frequency), "Insert description");
    }
    void print_parameters() override
    {
        // spdlog::info("TransitionMeasureTabuList - parameter frequency: {}", frequency);
    }
    void initialize(Runner* r)
    {}
    bool is_tabu(Runner* r)
    {
        // look in the transition measure table, if the hash of the move is there and it's frequency is above the given threshold, then return true, else it is false
        const auto& current_move = r->current_move_value->GetMove();
        size_t current_move_hash = r->ne->HashMove(current_move);
        if (transition_measure_table.contains(current_move_hash))
        {
            if (transition_measure_table[current_move_hash] / r->iteration >= frequency)
            {
#if !defined(NDEBUG)
                std::ostringstream oss;
                oss << current_move;
                spdlog::debug("TransitionMeasureTabuList - is tabu - {} Is tabu and the key in the hash table, with frequency {}", oss.str(), transition_measure_table[current_move_hash]);
#endif
                return true;
            }
        }
        return false;
    }
    void update(Runner* r)
    {
        // if the hash of the current move is present, then add 1, else it is equal to 1
        const auto& best_move = r->best_move_value->GetMove();
        size_t best_move_hash = r->ne->HashMove(best_move);
        if(transition_measure_table.contains(best_move_hash))
        {
            transition_measure_table[best_move_hash] =  transition_measure_table[best_move_hash] + 1;
        }
        else
        {
            transition_measure_table[best_move_hash] = 1;
        }
#if !defined(NDEBUG)
        for (auto p : transition_measure_table)
        {
            spdlog::debug("Update {} ---> {}", p.first, p.second);
        }
#endif
    }
    Move least_tabu(Runner* r)
    {
        // return the move that has the lower measure
        // thing is in transition_measure_table, I am saving possibly just part of the move, not the entire move, so how can I go back?
        return r->ne->RandomMove(r->current_solution_value->GetSolution());
    }
protected:
    std::map<size_t, size_t> transition_measure_table;
    double frequency;
};

template <class Runner>
class FooSchemeTabuList : public Parametrized
{
    using Move = typename Runner::Move;
    using T = typename Runner::T;
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("phi", po::value<size_t>(&phi), "Insert description")
            ("ita", po::value<size_t>(&ita), "Insert description")
            ("bi", po::value<T>(&bi), "Insert description");

    }
    void print_parameters() override
    {
        // spdlog::info("FooSchemeTabuList - parameter phi: {}", phi);
        // spdlog::info("FooSchemeTabuList - parameter ita: {}", ita);
        // spdlog::info("FooSchemeTabuList - parameter bi: {}", bi);
        // spdlog::info("FooSchemeTabuList - parameter current: {}", current);
        // spdlog::info("FooSchemeTabuList - parameter last_it_update: {}", last_it_update);
        // spdlog::info("FooSchemeTabuList - parameter max_current_size: {}", max_current_size);
        // spdlog::info("FooSchemeTabuList - parameter min_initialized: {}", min_initialized);
        // spdlog::info("FooSchemeTabuList - parameter max_initialized: {}", max_initialized);
    }
    void initialize(Runner* r)
    {
        current = 0;
        last_it_update = 0;
        max_current_size = ita;
        min_initialized = false;
        max_initialized = false;
    }
    bool is_tabu(Runner* r)
    {
        const auto& current_move = r->current_move_value->GetMove();
        const auto& current_solution = r->current_move_value->GetSolution();
        for (const auto& tl_move : tabu_moves)
        {
            if (r->ne->Inverse(current_solution, current_move, tl_move))
            {
#if !defined(NDEBUG)
                spdlog::debug("FooSchemeTabuList - move is tabu");
#endif
                return true;
            }
        }
        return false;
    }
    void update(Runner* r)
    {
        if(r->iteration == 0)
        {
            max_current_size = ita;
        }
        const Move& best_move = r->best_move_value->GetMove();
        const auto& best_move_cost = r->best_move_value->AggregatedCost();
        
        if (!min_initialized && !max_initialized)
        {
            min_o = best_move_cost;
            max_o = best_move_cost;
            min_initialized = true;
            max_initialized = true;
        }
        else
        {
            if (best_move_cost < min_o)
                min_o = best_move_cost;
            if(best_move_cost > max_o)
                max_o = best_move_cost;
            assert(min_o <= max_o);
        }
        
        if(r->iteration - last_it_update < phi)
        {
            add_or_update_current(r, best_move);
        }
        else
        {
            update_size(r, best_move);
        }
    }
    Move least_tabu(Runner* r)
    {
        return tabu_moves[current];
    }
protected:
    size_t current;
    size_t last_it_update;
    bool min_initialized, max_initialized;
    T min_o, max_o;
    std::vector<Move> tabu_moves;
    size_t max_current_size;
    // parameters
    size_t phi, ita;
    T bi;
    
    void add_or_update_current(Runner* r, Move best_move)
    {
#if !defined(NDEBUG)
        spdlog::debug("size is {} // current max size is {} // ita is ", tabu_moves.size(), max_current_size, ita);
#endif
        if (tabu_moves.size() < max_current_size)
        {
            tabu_moves.push_back(best_move);
            
        }
        else
        {
            tabu_moves[current] = best_move;
            current = (current + 1) % max_current_size;
        }
    }
    
    void update_size(Runner* r, Move best_move)
    {
        if (max_o - min_o < bi)
        {
            max_current_size = max_current_size + ita;
        }
        else
        {
            if (tabu_moves.size() > 0)
            {
                tabu_moves.erase(tabu_moves.begin() + current);
                max_current_size = max_current_size - 1;
                current = (current + 1) % max_current_size;
            }
        }
         max_initialized = false;
         min_initialized = false;
         last_it_update = r->iteration;
    }
};

template <class Runner>
class RandomFooSchemeTabuList : public Parametrized
{
    using Move = typename Runner::Move;
    using T = typename Runner::T;
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("max-phi", po::value<size_t>(&max_phi), "Insert description")
            ("min-phi", po::value<size_t>(&min_phi), "Insert description")
            ("max-ita", po::value<size_t>(&max_ita), "Insert description")
            ("min-ita", po::value<size_t>(&min_ita), "Insert description")
            ("max-bi", po::value<T>(&max_bi), "Insert description")
            ("min-bi", po::value<T>(&min_bi), "Insert description");

    }
    void print_parameters() override
    {
        // spdlog::info("RandomFooSchemeTabuList - parameter min-phi: {}", min_phi);
        // spdlog::info("RandomFooSchemeTabuList - parameter max-phi: {}", max_phi);
        // spdlog::info("RandomFooSchemeTabuList - parameter min-ita: {}", min_ita);
        // spdlog::info("RandomFooSchemeTabuList - parameter max-ita: {}", max_ita);
        // spdlog::info("RandomFooSchemeTabuList - parameter min-bi: {}", min_bi);
        // spdlog::info("RandomFooSchemeTabuList - parameter max-bi: {}", max_bi);
        // spdlog::info("RandomFooSchemeTabuList - parameter current: {}", current);
        // spdlog::info("RandomFooSchemeTabuList - parameter last_it_update: {}", last_it_update);
        // spdlog::info("RandomFooSchemeTabuList - parameter max_current_size: {}", max_current_size);
        // spdlog::info("RandomFooSchemeTabuList - parameter min_initialized: {}", min_initialized);
        // spdlog::info("RandomFooSchemeTabuList - parameter max_initialized: {}", max_initialized);
        // // spdlog::info("RandomFooSchemeTabuList - parameter random seed: {}", rng.seed());
    }
    void initialize(Runner* r)
    {
        current = 0;
        last_it_update = 0;
        // set ita, phi, bi for the first round
        rng.seed(r->random_seed);
        std::uniform_int_distribution<size_t> phi_dist(min_phi, max_phi);
        phi = phi_dist(rng);
        
        std::uniform_int_distribution<size_t> ita_dist(min_ita, max_ita);
        ita = ita_dist(rng);
        
        std::uniform_int_distribution<T> bi_dist(min_bi, max_bi);
        bi = bi_dist(rng);
        
        max_current_size = ita;
        min_initialized = false;
        max_initialized = false;
    }

    bool is_tabu(Runner* r)
    {
        const auto& current_move = r->current_move_value->GetMove();
        const auto& current_solution = r->current_move_value->GetSolution();
        for (const auto& tl_move : tabu_moves)
        {
            if (r->ne->Inverse(current_solution, current_move, tl_move))
            {
#if !defined(NDEBUG)
                spdlog::debug("RandomFooSchemeTabuList - move is tabu");
#endif
                return true;
            }
        }
        return false;
    }
    void update(Runner* r)
    {
        if(r->iteration == 0)
        {
            max_current_size = ita;
        }
        const Move& best_move = r->best_move_value->GetMove();
        const auto& best_move_cost = r->best_move_value->AggregatedCost();
        
        if (!min_initialized && !max_initialized)
        {
            min_o = best_move_cost;
            max_o = best_move_cost;
            min_initialized = true;
            max_initialized = true;
        }
        else
        {
            if (best_move_cost < min_o)
                min_o = best_move_cost;
            if(best_move_cost > max_o)
                max_o = best_move_cost;
        }
        
        if(r->iteration - last_it_update < phi)
        {
            add_or_update_current(r, best_move);
        }
        else
        {
            update_size(r, best_move);
        }
    }
    Move least_tabu(Runner* r)
    {
        return tabu_moves[current];
    }
protected:
    size_t current;
    size_t last_it_update;
    bool min_initialized, max_initialized;
    T min_o, max_o;
    std::vector<Move> tabu_moves;
    size_t max_current_size;
    mutable std::mt19937_64 rng;
    // parameters
    size_t max_ita, min_ita, ita;
    size_t max_phi, min_phi, phi;
    T max_bi, min_bi, bi;
    
    void add_or_update_current(Runner* r, Move best_move)
    {
#if !defined(NDEBUG)
        spdlog::debug("size is {} // current max size is {} // ita is ", tabu_moves.size(), max_current_size, ita);
#endif
        if (tabu_moves.size() < max_current_size)
        {
            tabu_moves.push_back(best_move);
            
        }
        else
        {
            tabu_moves[current] = best_move;
            current = (current + 1) % max_current_size;
        }
    }
    
    void update_size(Runner* r, Move best_move)
    {
        
        if (max_o - min_o < bi)
        {
            max_current_size = max_current_size + ita;
        }
        else
        {
            if (tabu_moves.size() > 0)
            {
                tabu_moves.erase(tabu_moves.begin() + current);
                max_current_size = max_current_size - 1;
                current = (current + 1) % max_current_size;
            }
        }
         max_initialized = false;
         min_initialized = false;
         last_it_update = r->iteration;
        
        std::uniform_int_distribution<size_t> phi_dist(min_phi, max_phi);
        phi = phi_dist(rng);
        
        std::uniform_int_distribution<size_t> ita_dist(min_ita, max_ita);
        ita = ita_dist(rng);
        
        std::uniform_int_distribution<T> bi_dist(min_bi, max_bi);
        bi = bi_dist(rng);
    }
};

// TODO: define the proper concept for AspirationCriterion
template <class Runner>
class AspirationByObjective : public Parametrized
{
public:
    bool is_tabu_status_overridden(Runner* r)
    {
        if (*(r->current_move_value) < *(r->best_solution_value))
        {
#if !defined(NDEBUG)
            spdlog::debug("AspirationByObjective - Tabu status overriden");
#endif
            return true;
            r->metric_aspiration_used + 1;
        }
        else
            return false;
    }
    void update(Runner* r)
    {}
    bool use_least_tabu(Runner* r)
    {
#if !defined(NDEBUG)
        spdlog::debug("AspirationByObjective - Use least tabu is false");
#endif
        return false;
    }
protected:
};

template <class Runner>
class AspirationByDefault : public Parametrized
{
public:
    bool is_tabu_status_overridden(Runner* r)
    {
        // you never override
        return false;
    }
    void update(Runner* r)
    {}
    bool use_least_tabu(Runner* r)
    {
#if !defined(NDEBUG)
        spdlog::debug("AspirationByDefault - Use least tabu is true");
#endif
        return true;
    }
protected:
};

// TODO: define the proper concept for StopExploration
template <class Runner>
class StopExplorationBestImprovement : public Parametrized
{
public:
    bool has_to_stop(Runner* r)
    {
        return false;
    }
    void update(Runner* r)
    {}
    void initialize(Runner* r)
    {}
protected:
};

template <class Runner>
class StopExplorationFirstImprovement : public Parametrized
{
public:
    bool has_to_stop(Runner* r)
    {
        if (*(r->current_move_value) < *(r->best_solution_value))
        {
#if !defined(NDEBUG)
            spdlog::debug("StopExplorationFirstImprovement - Stopping at best improvement");
#endif
            return true;
        }
        return false;
    }
    void update(Runner* r)
    {}
    void initialize(Runner* r)
    {}
protected:
};

template <class Runner>
class StopExplorationAspirationPlus : public Parametrized
{
    using T = typename Runner::T;
public:
    void add_parameter(po::options_description& opt) override
    {
        opt.add_options()
            ("theta-asp", po::value<double>(&theta), "Insert description")
            ("min", po::value<size_t>(&min), "Insert description")
            ("max", po::value<size_t>(&max), "Insert description")
            ("plus", po::value<size_t>(&plus), "Insert description");
    }
    void print_parameters() override
    {
        // spdlog::info("StopExplorationAspirationPlus - parameter theta: {}", theta);
        // spdlog::info("StopExplorationAspirationPlus - parameter min: {}", min);
        // spdlog::info("StopExplorationAspirationPlus - parameter max: {}", max);
        // spdlog::info("StopExplorationAspirationPlus - parameter plus: {}", plus);
    }
    bool has_to_stop(Runner* r)
    {
        return (number_of_neighbors >= max) || (first_found && number_of_neighbors - first_under_threshold >= plus && number_of_neighbors >= min);
    }
    void update(Runner* r)
    {
        number_of_neighbors++;
        if (!first_found && r->current_move_value->AggregatedCost() < threshold)
        {
#if !defined(NDEBUG)
            spdlog::debug("StopExplorationAspirationPlus - Found value under threshold");
#endif
            first_found = true;
            first_under_threshold = number_of_neighbors;
        }
    }
    void initialize(Runner* r)
    {
        // FIMXE: this will work only with aggregated cost, to think about for lexicographic and pareto objectives
        threshold = r->best_solution_value->AggregatedCost() * theta;
        number_of_neighbors = 0;
        first_found = false;
        assert(theta >= 1.0);
        assert(min > 0 && min < max);
    }
protected:
    T threshold;
    double theta;
    bool first_found;
    size_t min, max, plus;
    size_t number_of_neighbors, first_under_threshold;
};

// TODO: define the proper concept for SelectMove
template <class Runner>
class SelectMoveRandom : public Parametrized
{
    using Move = typename Runner::Move;
public:
    auto select(Runner* r)
    {
        return r->ne->CreateMoveValue(*r->current_solution_value, r->ne->RandomMove(r->current_solution_value->GetSolution()));
    }
protected:
};

template <class Runner>
class SelectMoveScanningAll : public Parametrized
{
    using Move = typename Runner::Move;
    using MoveValue = typename Runner::MoveValue;
public:
    auto select(Runner* r)
    {
        bool best_move_value_initialized = false;
        for (auto mv : r->ne->Neighborhood(r->current_solution_value->GetSolution()))
        {
            current_move_value = std::make_shared<MoveValue>(r->ne->CreateMoveValue(*(r->current_solution_value), mv));
            if (!best_move_value_initialized || *current_move_value < *best_move_value)
            {
                best_move_value = std::make_shared<MoveValue>(*current_move_value);
                best_move_value_initialized = true;
            }
        }
        return *best_move_value;
    }
protected:
    std::shared_ptr<MoveValue> best_move_value, current_move_value;
};

// TODO: define the proper concept for AcceptMove
template <class Runner>
class AcceptMoveAlways : public Parametrized
{
    using Move = typename Runner::Move;
public:
    bool accept(Runner* r)
    {
        return true;
    }
protected:
};

template <class Runner>
class AcceptMoveImproveOrEqual : public Parametrized
{
    using Move = typename Runner::Move;
public:
    bool accept(Runner* r)
    {
        
        return *(r->current_move_value) <= *(r->current_solution_value);
    }
protected:
};

template <class Runner>
class AcceptMoveImprove : public Parametrized
{
    using Move = typename Runner::Move;
public:
    bool accept(Runner* r)
    {
        return *(r->current_move_value) < *(r->current_solution_value);
    }
protected:
};

}



// end --- components.hh --- 



// begin --- runner.hh --- 

//
//  runner.hh
//  pfsp-ls
//
//  Created by Luca Di Gaspero on 24/07/23.
//

#pragma once

#include <thread>
#include <future>
#include <chrono>
#include <atomic>
#include <boost/program_options.hpp>

namespace po = boost::program_options;

namespace easylocal {

template <SolutionManagerT SolutionManager>
class AbstractRunner
{
public:
    using Input = typename SolutionManager::Input;
    using SolutionValue = typename SolutionManager::SolutionValue;
    
    virtual SolutionValue Run(std::shared_ptr<const Input> in, std::chrono::milliseconds timeout) = 0;
    virtual void SetParameters(po::variables_map& /* vm */, std::vector<std::string> /* to_pass_further */) {};
};


template <SolutionManagerT SolutionManager, NeighborhoodExplorerT NeighborhoodExplorer>
class Runner : public AbstractRunner<SolutionManager>
{
public:
    using Input = typename SolutionManager::Input;
    using Solution = typename SolutionManager::Solution;
    using T = typename SolutionManager::T;
    using CostStructure = typename SolutionManager::CostStructure ;
    using Move = typename NeighborhoodExplorer::Move;
    using SolutionValue = typename SolutionManager::SolutionValue;
    
    
protected:
    Runner(std::shared_ptr<const SolutionManager> sm, std::shared_ptr<const NeighborhoodExplorer> ne) : sm(sm), ne(ne) {}
    // virtual void SetParameters(po::variables_map& vm, std::vector<std::string> to_pass_further){};
    
public:
    
    SolutionValue Run(std::shared_ptr<const Input> in, std::chrono::milliseconds timeout) override
    {
        std::packaged_task<void(std::shared_ptr<const Input> in)> running_task([this](std::shared_ptr<const Input> in) {
            this->ResetStopRun();
            this->Go(in);
        });
        auto future = running_task.get_future();
        std::thread thr(std::move(running_task), in);
        future.wait_for(timeout);
        stop_run = true;
        thr.join();
        return *(final_solution_value);
    }
    
    inline void Run(std::shared_ptr<const Input> in)
    {
        this->Go(in);
    }
    
protected:
    
    virtual void Go(std::shared_ptr<const Input> in) = 0;
    
    inline void ResetStopRun()
    {
        stop_run = false;
    }
    
    inline bool StopRun() const
    {
        return stop_run;
    }
    
public:
    std::shared_ptr<const SolutionManager> sm;
    std::shared_ptr<const NeighborhoodExplorer> ne;
    std::atomic_bool stop_run;
    std::shared_ptr<SolutionValue> final_solution_value;
};
}



// end --- runner.hh --- 


#include <iostream>
#include <thread>
#include <future>
#include <chrono>
#include <iterator>
#include <memory>
#include <functional>
#include <boost/program_options.hpp>

namespace po = boost::program_options;


namespace easylocal {

// TODO: review all the names of templates / classes

template <SolutionManagerT SolutionManager, NeighborhoodExplorerT NeighborhoodExplorer, template <class R> class TerminationCriterion, template <class R> class TabuList, template <class R> class AspirationCriterion, template <class R> class StopExplorationCriterion, template <class R> class NeighborhoodGenerator>
class TabuSearch : public Runner<SolutionManager, NeighborhoodExplorer>
{
public:
    using Input = typename SolutionManager::Input;
    using Solution = typename SolutionManager::Solution;
    using T = typename SolutionManager::T;
    using CostStructure = typename SolutionManager::CostStructure;
    using Move = typename NeighborhoodExplorer::Move;
    using SelfClass = TabuSearch<SolutionManager, NeighborhoodExplorer, TerminationCriterion, TabuList, AspirationCriterion, StopExplorationCriterion, NeighborhoodGenerator>;
    
    using SolutionValue = typename SolutionManager::SolutionValue;
    using MoveValue = typename NeighborhoodExplorer::MoveValue;
    
    TabuSearch(std::shared_ptr<const SolutionManager> sm, std::shared_ptr<const NeighborhoodExplorer> ne, size_t random_seed) : Runner<SolutionManager, NeighborhoodExplorer>(sm, ne), random_seed(random_seed) {}
    
    void SetParameters(po::variables_map& vm, std::vector<std::string> to_pass_further) override
    {
        po::options_description desc("Set of parameters associated with the required TS.");
        termination.add_parameter(desc);
        tabu_list.add_parameter(desc);
        aspiration.add_parameter(desc);
        stop_exploration.add_parameter(desc);
        neighborhood_generator.add_parameter(desc);
        po::store(po::command_line_parser(to_pass_further).options(desc).run(), vm);
        po::notify(vm);
    }
    
protected:

    virtual void Go(std::shared_ptr<const Input> in) override
    {
        tabu_list.initialize(this);
        PrintParameters();
        current_solution_value = std::make_shared<SolutionValue>(this->sm->CreateSolutionValue(this->sm->InitialSolution(in)));
        best_solution_value = std::make_shared<SolutionValue>(*current_solution_value);
        // TODO: 1. implement aspiration plus and elite candidate plus strategies
        while (!termination.terminate(this) && !this->StopRun()) //TODO: StopRun qui? da altre parti?
        {
            bool best_move_value_initialized = false;
            stop_exploration.initialize(this);
            try
            {
                for (auto _cmv : neighborhood_generator.generate_moves(this))
                {
                    current_move_value = _cmv;
                    if (tabu_list.is_tabu(this) && !aspiration.is_tabu_status_overridden(this))
                    {
                        continue;
                    }
                    if (!best_move_value_initialized || *current_move_value < *best_move_value)
                    {
                        best_move_value = std::make_shared<MoveValue>(*current_move_value);
                        best_move_value_initialized = true;
                    }
                    stop_exploration.update(this);
                    if (stop_exploration.has_to_stop(this))
                    {
                        break;
                    }
                }
            }
            catch (EmptyNeighborhood)
            {
#if !defined(NDEBUG)
                spdlog::debug("empty neighborhood encountered while exploring");
#endif
                break;
            }
            
            if (!best_move_value_initialized)
            {
                // spdlog::warn("HERE");
                if (!aspiration.use_least_tabu(this))
                {
#if !defined(NDEBUG)
                    spdlog::debug("best_move_value_initialized not initialized and you are not using least_tabu_move");
#endif
                    break;
                }
                else
                {
                    best_move_value = std::make_shared<MoveValue>(this->ne->CreateMoveValue(*current_solution_value, tabu_list.least_tabu(this)));
                }
            }
            
            current_solution_value = std::make_shared<SolutionValue>(*best_move_value);
            std::ostringstream oss;
            oss << (*(current_solution_value->GetSolution()));
            // std::cout << oss.str() << std::endl;
            spdlog::info("{} --> {}", oss.str(), current_solution_value->AggregatedCost());
            if (*current_solution_value < *best_solution_value)
            {
                best_solution_value = std::make_shared<SolutionValue>(*current_solution_value);
                idle_iteration = 0;
            }
            else
            {
                idle_iteration = idle_iteration + 1;
            }
            // update iterations
            iteration = iteration + 1;
            tabu_list.update(this);
            stop_exploration.update(this);
            // To remove later
#if !defined(NDEBUG)
            //std::ostringstream oss;
            // oss << best_move_value->GetMove();
            //auto values = current_solution_value->GetValues();
            //spdlog::debug("TS - move selected {} / {} {} / {} ", iteration, idle_iteration, oss.str(), spdlog::fmt_lib::join(values, ", "));
#endif
        }
        // post processings
        // auto values = best_solution_value->GetValues();
        // std::ostringstream oss;
        // oss << (*(best_solution_value->GetSolution()));
        // spdlog::info("{} ---> ({})", oss.str(), spdlog::fmt_lib::join(values, ", "));
        // spdlog::info("Idle iterations: {} // Total iterations: {}", idle_iteration, iteration);
#if !defined(NDEBUG)
        spdlog::debug("Very end: iteration: {} // idle_iteration:{}", iteration, idle_iteration);
        spdlog::debug("Checking current solution");
        assert(current_solution_value->CheckValues());
        spdlog::debug("Checking best solution");
#endif
        assert(best_solution_value->CheckValues());
        this->final_solution_value = std::make_shared<SolutionValue>(*best_solution_value);
    }
    void PrintParameters()
    {
        // spdlog::info("Paremeter random_seed: {}", random_seed);
        termination.print_parameters();
        tabu_list.print_parameters();
        aspiration.print_parameters();
        stop_exploration.print_parameters();
        neighborhood_generator.print_parameters();
    }
public:
    // object data
    size_t iteration = 0, idle_iteration = 0;
    size_t metric_aspiration_used = 0;
    size_t random_seed;
    std::shared_ptr<SolutionValue> current_solution_value, best_solution_value;
    std::shared_ptr<MoveValue> current_move_value, best_move_value;
protected:
    // parametrized criteria
    TerminationCriterion<SelfClass> termination;
    TabuList<SelfClass> tabu_list;
    AspirationCriterion<SelfClass> aspiration;
    StopExplorationCriterion<SelfClass> stop_exploration;
    NeighborhoodGenerator<SelfClass> neighborhood_generator;
};

}


// end --- tabu-search.hh --- 



// begin --- version.hh --- 

#pragma once

#define EASYLOCAL_VER_MAJOR 4
#define EASYLOCAL_VER_MINOR 0
#define EASYLOCAL_VER_PATCH 0

#define EASYLOCAL_TO_VERSION(major, minor, patch) (major * 10000 + minor * 100 + patch)
#define EASYLOCAL_VERSION EASYLOCAL_TO_VERSION(EASYLOCAL_VER_MAJOR, EASYLOCAL_VER_MINOR, EASYLOCAL_VER_PATCH)

// end --- version.hh --- 



// begin --- multi-modal-neighborhood-explorer.hh --- 

//
//  multi-modal-neighborhood-explorer.hh
//  poc
//
//  Created by Luca Di Gaspero on 09/03/23.
//

#pragma once
#include <random>
#include <variant>

namespace easylocal {
  // TODO: define the neighborhood concept later and the proper parameters, in particular the same_as for the solution manager
  // TODO: consider whether to pass also Input or not or to simplify the solution class concept having an alternative definition that is Input-less
  // TODO: consider whether the movecoststructure-like related functions should be outsourced in a different class
  template <SolutionManagerT _SolutionManager, class SelfClass, typename ...NeighborhoodExplorers>
  requires (NeighborhoodExplorerT<NeighborhoodExplorers> && ...)
class UnionNeighborhoodExplorer : public std::enable_shared_from_this<SelfClass>
  {
  public:
    using SolutionManager = _SolutionManager;
    using Input = typename SolutionManager::Input;
    using Solution = typename SolutionManager::Solution;
    using T = typename SolutionManager::T;
    using Move = std::variant<typename NeighborhoodExplorers::Move...>;
    using CostStructure = typename SolutionManager::CostStructure;
    friend class MoveValue<Input, Solution, T, CostStructure, SelfClass>;
    using MoveValue = MoveValue<Input, Solution, T, CostStructure, SelfClass>;
    using SolutionValue = SolutionValue<Input, Solution, T, CostStructure>;
    using ThisClass = UnionNeighborhoodExplorer<SolutionManager, SelfClass, NeighborhoodExplorers...>;

    // Union specific    
    using GeneratorMove = std::variant<Generator<typename NeighborhoodExplorers::Move>...> ;
    using GeneratorIterMove = std::variant<typename Generator<typename NeighborhoodExplorers::Move>::Iter...>;
    
      UnionNeighborhoodExplorer(std::shared_ptr<SolutionManager> sm) : nhes{NeighborhoodExplorers(sm)...}, cmv{std::forward<NeighborhoodExplorers>(NeighborhoodExplorers(sm))...} /*,  ci{std::forward<NeighborhoodExplorers>(NeighborhoodExplorers(sm))...},
          chm{std::forward<NeighborhoodExplorers>(NeighborhoodExplorers(sm))...} */
    {
//      delta_cost_components.resize(sm->Components());
    }
        
    Generator<Move> Neighborhood(std::shared_ptr<const Solution> sol) const
    {
      for (size_t i = 0; i < sizeof...(NeighborhoodExplorers); ++i)
      {
        auto gen = perform(nhes, i, CaptureVariantNeighborhood{sol}).generator;
        // since the type of gen is a variant of the move generators, the only way to obtain the iterators is by using std::visit
        auto iterator = std::visit([](auto&& arg)->GeneratorIterMove { return arg.begin(); }, gen);
        while (true)
        {
          // the use of visit allows to call the iterator operators with the right type in the variant
          auto value = std::visit([](auto&& arg)->Move { return *arg; }, iterator);
          co_yield value;
          std::visit([](auto&& arg) { ++arg; }, iterator);
          if (std::visit([](auto&& arg)->bool { return arg == std::default_sentinel_t{}; }, iterator))
            break;
        }
      }
    }
    
    // FIXME: capture empty neighborhood exceptions
    Move RandomMove(std::shared_ptr<const Solution> sol) const
    {
        // FIXME: random seed!
      std::random_device dev;
      std::mt19937 rng(dev());
      std::uniform_int_distribution<std::mt19937::result_type> dist_move(0, sizeof...(NeighborhoodExplorers) - 1);
      size_t pos = dist_move(rng);
      CaptureVariantRandom cv{sol, std::optional<Move>()};
      return perform(nhes, pos, cv).move.value();
    }
    
    void MakeMove(std::shared_ptr<Solution> sol, const Move& mv) const
    {
      std::visit([&sol, this](auto&& arg) { this->cmv.MakeMove(sol, arg); }, mv);
    }
            
      // Fallback method to handle the case where the concept is not met
      bool InverseMove(...) const
      {
          static_assert(sizeof...(NeighborhoodExplorers) == 0, "The Inverse method is not available because one or more neighborhood types do not have the required Inverse function.");
          return false;
      }
      
      // Method enabled only if all NeighborhoodExplorerss satisfy has_inverse_move
      // template <typename = std::enable_if_t<(has_inverse_move<NeighborhoodExplorers> && ...)>>
      bool InverseMove(std::shared_ptr<const Solution> sol, const Move& mv1, const Move& mv2) const requires (has_inverse_move<NeighborhoodExplorers> && ...)
      {
          return false;
//          return std::visit([&sol, this](auto&& arg1, auto&& arg2) { return this->ci.Inverse(sol, arg1, arg2); }, mv1, mv2);
      }
//      
//      size_t HashMove(const Move& mv) const
//      {
//          return std::visit([this](auto&& arg) { return this->chm.HashMove(arg); }, mv);
//      }

    
  protected:
    struct CaptureVariantRandom
    {
      template <typename T>
      void operator()(T&& t)
      {
        move = Move{t.RandomMove(sol)};
      }
      std::shared_ptr<const Solution> sol;
      std::optional<Move> move;
    };
    
    struct CaptureVariantNeighborhood
    {
      template <typename T>
      void operator()(T&& t)
      {
        generator = GeneratorMove{t.Neighborhood(sol)};
      }
      std::shared_ptr<const Solution> sol;
      GeneratorMove generator;
    };
    
    struct CaptureMakeMove : NeighborhoodExplorers...
    {
      using NeighborhoodExplorers::MakeMove...;
    };

// FIXME: find a suitable way to work with inverse and hash
      
//  struct CaptureInverse : NeighborhoodExplorers...
//  {
//    using NeighborhoodExplorers::Inverse...;
//  };
//      
//      struct CaptureHashMove : NeighborhoodExplorers...
//      {
//        using NeighborhoodExplorers::HashMove...;
//      };
    
    std::tuple<NeighborhoodExplorers...> nhes;
    CaptureMakeMove cmv;
//    CaptureInverse ci;
//    CaptureHashMove chm;
  public:
    
    MoveValue CreateMoveValue(const SolutionValue& sv, const Move& mv) const
    {
      return { this->shared_from_this(), sv, mv, sv.size() };
    }
    
    template <class BasicMove, DeltaCostComponentT<Input, Solution, T, BasicMove> DCC>
    inline void AddDeltaCostComponent(DCC& dcc, size_t i)
    {
        constexpr size_t nhe_index = variant_index<size_t(0), BasicMove, typename NeighborhoodExplorers::Move...>();
        static_assert(nhe_index < sizeof...(NeighborhoodExplorers), "Wrong move type, it dows not belong to the set of types handled by the Union Neighborhood Explorer");
      std::get<nhe_index>(nhes).AddDeltaCostComponent(dcc, i);
    }
      // FIXME: restate
//  protected:

  protected:
    template<std::size_t... I>
    bool callHasDeltaCostComponent(size_t i, const Move& move, std::index_sequence<I...>) const
    {
        bool result = false;
        // Using fold expression to call the correct neighborhood explorer's HasDeltaCostComponent
        (..., ([&]() -> bool {
            if (const auto* ptr = std::get_if<std::variant_alternative_t<I, Move>>(&move))
            {
                result = std::get<I>(nhes).HasDeltaCostComponent(i, *ptr);
                return true;
            }
            return false;
        })());
        return result;
    }
      
      template<std::size_t... I>
      T callDeltaCostComponent(size_t i, std::shared_ptr<const Solution> sol, const Move& move, std::index_sequence<I...>) const
      {
          T result = T{0};
          // Using fold expression to call the correct neighborhood explorer's HasDeltaCostComponent
          (..., ([&]() -> bool {
              if (const auto* ptr = std::get_if<std::variant_alternative_t<I, Move>>(&move))
              {
                  result = std::get<I>(nhes).ComputeDeltaCost(sol, *ptr, i);
                  return true;
              }
              return false;
          })());
          return result;
      }
      
  public:
    T ComputeDeltaCost(std::shared_ptr<const Solution> sol, const Move& mv, size_t i) const
    {
        return std::visit([&](auto&&) -> T {
            return this->callDeltaCostComponent(i, sol, mv, std::index_sequence_for<typename NeighborhoodExplorers::Move...>{});
        }, mv);
    }
    
    // FIXME: it should go through the variant move to establish if the specific element in the tuple is nullptr or not
    bool HasDeltaCostComponent(size_t i, const Move& mv) const
    {
        return std::visit([&](auto&&) -> bool {
            return this->callHasDeltaCostComponent(i, mv, std::index_sequence_for<typename NeighborhoodExplorers::Move...>{});
        }, mv);
    }
    
    // std::vector<std::tuple<std::unique_ptr<DeltaCostComponent<Input, Solution, T, typename NeighborhoodExplorers::Move>>...>> delta_cost_components;
  };
}


// end --- multi-modal-neighborhood-explorer.hh --- 



// begin --- plahc.hh --- 

//
//  lahc.hh
//  poc
//
//  Created by Luca Di Gaspero on 13/03/23.
//

#pragma once
#include <iostream>
#include <iterator>
#include <memory>
#include <spdlog/spdlog.h>

namespace easylocal {
  template <SolutionManagerT SolutionManager, NeighborhoodExplorerT NeighborhoodExplorer>
  class PLAHC : public Runner<SolutionManager, NeighborhoodExplorer>
  {
  public:
    using Input = typename Runner<SolutionManager, NeighborhoodExplorer>::Input;
    using Solution = typename Runner<SolutionManager, NeighborhoodExplorer>::Solution;
    using T = typename Runner<SolutionManager, NeighborhoodExplorer>::T;
    using CostStructure = typename Runner<SolutionManager, NeighborhoodExplorer>::CostStructure ;
    using Move = typename Runner<SolutionManager, NeighborhoodExplorer>::Move;
    
    PLAHC(std::shared_ptr<const SolutionManager> sm, std::shared_ptr<const NeighborhoodExplorer> ne, size_t history_length) : Runner<SolutionManager, NeighborhoodExplorer>(sm, ne), history_length(history_length) {}  
  protected:

    virtual void Go(std::shared_ptr<const Input> in) override
    {
      size_t iteration = 0, idle_iteration = 0;
      this->ResetStopRun();
      std::vector<SolutionValue<Input, Solution, T, CostStructure>> history;
      history.reserve(history_length);
      for (size_t i = 0; i < history_length; ++i)
        history.push_back(this->sm->CreateSolutionValue(this->sm->InitialSolution(in)));
      iteration = 0;
      idle_iteration = 0;
      size_t index = 0;
      auto current_solution_value = history[0];
      while ((iteration < max_iterations || idle_iteration <= 0.02 * iteration) && !this->StopRun())
      {
        size_t next_index = (index + 1) % history.size();
        auto current_move_value = this->ne->CreateMoveValue(current_solution_value, this->ne->RandomMove(current_solution_value.GetSolution()));
        if (current_move_value < current_solution_value)
        {
          history[index] = current_move_value;
          current_solution_value = history[next_index];
          index = (index + 1) % history.size();
          idle_iteration = 0;
        }
        else if (current_move_value < history[next_index])
        {
          current_solution_value = history[next_index];
          history[next_index] = current_move_value;
          index = (index + 2) % history.size();
          idle_iteration = 0;
        } else {
          current_solution_value = history[next_index];
          index = (index + 1) % history.size();
          idle_iteration++;
        }
        iteration++;
      }
      // post process solutions to get the pareto set
      std::vector<SolutionValue<Input, Solution, T, CostStructure>> pareto_front;
      for (size_t i = 0; i < history.size(); ++i)
      {
        bool non_dominated = true, drop_equal_solutions = false;
        for (size_t j = 0; j < history.size(); ++j)
        {
          if (i == j)
            continue;
          if (history[i] > history[j])
          {
            non_dominated = false;
            break;
          }
          if constexpr(std::equality_comparable<Solution>)
          {
            if (*history[i].GetSolution() == *history[j].GetSolution() && i > j)
            {
              drop_equal_solutions = true;
              break;
            }
          }
        }
        if (non_dominated && !drop_equal_solutions)
          pareto_front.emplace_back(history[i]);
      }
      spdlog::info("Pareto front size: {}", pareto_front.size());
      for (const auto& sol : pareto_front)
      {
        auto values = sol.GetValues();
//        std::copy(values.begin(), values.end(), std::ostream_iterator<T>(std::cout, " "));
//        std::cout << std::endl;
        std::ostringstream oss;
        oss << (*(sol.GetSolution()));
        spdlog::info("{} ---> ({})", oss.str(), spdlog::fmt_lib::join(values, ", "));

        assert(sol.CheckValues());
      }
      spdlog::info("Iterations: {}", iteration);
    }
  protected:
    // parameters
    size_t max_iterations = 1000000;
    size_t history_length;
  };
}


// end --- plahc.hh --- 



// begin --- plahc-one-chance.hh --- 

//
//  lahc.hh
//  poc
//
//  Created by Luca Di Gaspero on 13/03/23.
//

#pragma once
#include <iostream>
#include <thread>
#include <future>
#include <chrono>
#include <iterator>
#include <memory>

namespace easylocal {
  template <SolutionManagerT SolutionManager, NeighborhoodExplorerT NeighborhoodExplorer>
  class PLAHC_ONE_CHANCE
  {
  public:
    using Input = typename SolutionManager::Input;
    using Solution = typename SolutionManager::Solution;
    using T = typename SolutionManager::T;
    using CostStructure = typename SolutionManager::CostStructure;
    using Move = typename NeighborhoodExplorer::Move;
    
    PLAHC_ONE_CHANCE(std::shared_ptr<const SolutionManager> sm, std::shared_ptr<const NeighborhoodExplorer> ne, size_t history_length) : sm(sm), ne(ne), history_length(history_length) {}
  
    void Run(std::shared_ptr<const Input> in, std::chrono::milliseconds timeout)
    {
      std::packaged_task<void(std::shared_ptr<const Input> in)> running_task([this](std::shared_ptr<const Input> in) {
        this->Run(in);
      });
      auto future = running_task.get_future();
      std::thread thr(std::move(running_task), in);
      future.wait_for(timeout);
      stop_run = true;
      thr.join();
    }

    void Run(std::shared_ptr<const Input> in)
    {
      stop_run = false;
      std::vector<SolutionValue<Input, Solution, T, CostStructure>> history;
      history.reserve(history_length);
      for (size_t i = 0; i < history_length; ++i)
        history.push_back(sm->CreateSolutionValue(sm->InitialSolution(in)));
      iteration = 0;
      idle_iteration = 0;
      size_t index = 0;
      auto current_solution_value = history[0];
      while ((iteration < max_iterations || idle_iteration <= 0.02 * iteration) && !stop_run)
      {
        size_t next_index = (index + 1) % history.size();
        auto current_move_value = ne->CreateMoveValue(current_solution_value, ne->RandomMove(current_solution_value.GetSolution()));
        if (current_move_value < current_solution_value)
        {
          history[index] = current_move_value;
          current_solution_value = history[next_index];
          index = (index + 1) % history.size();
          idle_iteration = 0;
        }
        // else if (current_move_value < history[next_index])
        // {
        //   current_solution_value = history[next_index];
        //   history[next_index] = current_move_value;
        //   index = (index + 2) % history.size();
        //   idle_iteration = 0;
        // }
        else {
          current_solution_value = history[next_index];
          index = (index + 1) % history.size();
          idle_iteration++;
        }
        iteration++;
      }
      // post process solutions to get the pareto set
      std::vector<SolutionValue<Input, Solution, T, CostStructure>> pareto_front;
      for (size_t i = 0; i < history.size(); ++i)
      {
        bool non_dominated = true, drop_equal_solutions = false;
        for (size_t j = 0; j < history.size(); ++j)
        {
          if (i == j)
            continue;
          if (history[i] > history[j])
          {
            non_dominated = false;
            break;
          }
          if constexpr(std::equality_comparable<Solution>)
          {
            if (*history[i].GetSolution() == *history[j].GetSolution() && i > j)
            {
              drop_equal_solutions = true;
              break;
            }
          }
        }
        if (non_dominated && !drop_equal_solutions)
          pareto_front.emplace_back(history[i]);
      }
      std::cout << "Pareto front size: " << pareto_front.size() << std::endl;
      for (const auto& sol : pareto_front)
      {
        std::cout << *(sol.GetSolution()) << " ---> ";
        auto values = sol.GetValues();
        std::copy(values.begin(), values.end(), std::ostream_iterator<T>(std::cout, " "));
        std::cout << std::endl;
      }
      std::cout << "Iterations: " << iteration << std::endl;
    }
  protected:
    std::shared_ptr<const SolutionManager> sm;
    std::shared_ptr<const NeighborhoodExplorer> ne;
    size_t iteration = 0, idle_iteration = 0, max_iterations = 1000000;
    // parameter
    size_t history_length;
    std::atomic_bool stop_run;
  };
}


// end --- plahc-one-chance.hh --- 



// begin --- hill-climbing.hh --- 

//
//  hill-climbing.hh
//  pfsp-ls
//
//  Created by Luca Di Gaspero on 24/07/23.
//

#pragma once
#include <iostream>
#include <sstream>
#include <thread>
#include <future>
#include <chrono>
#include <iterator>
#include <memory>

namespace easylocal {

//template <class Runner>
//concept RunnerIdleIterT = has_basic_typedefs<Runner> &&
//requires(Runner) {
//    { Runner::idle_iteration } -> std::same_as<size_t&>;
//};


//template <RunnerIdleIterT Runner>
//class IdleIterationsTermination
//{
//public:
//    IdleIterationsTermination(size_t max_idle_iterations) : max_idle_iterations(max_idle_iterations) {}
//    
//    virtual bool operator()(Runner* r)
//    {
//        spdlog::debug("HERE in operator()");
//        return r->idle_iteration > max_idle_iterations;
//    }
//protected:
//    size_t max_idle_iterations;
//};

//template <RunnerIdleIterT Runner>
//class TotalIterationsTermination
//{
//public:
//    TotalIterationsTermination(size_t max_iterations) : max_iterations(max_iterations) {}
//    
//    virtual bool operator()(Runner* r)
//    {
//        spdlog::debug("HERE in operator() of TIT");
//        return r->iteration > max_iterations;
//    }
//protected:
//    size_t max_iterations;
//};

template <SolutionManagerT SolutionManager, NeighborhoodExplorerT NeighborhoodExplorer, template <class R> class TerminationCriterion = IdleIterationsTermination, template <class R> class SelectMove = SelectMoveRandom, template <class R> class AcceptMove = AcceptMoveImproveOrEqual>
class HillClimbing : public Runner<SolutionManager, NeighborhoodExplorer>
{
public:
    using Input = typename SolutionManager::Input;
    using Solution = typename SolutionManager::Solution;
    using T = typename SolutionManager::T;
    using CostStructure = typename SolutionManager::CostStructure;
    using Move = typename NeighborhoodExplorer::Move;
    using SelfClass = HillClimbing<SolutionManager, NeighborhoodExplorer, TerminationCriterion, SelectMove, AcceptMove>;
    
    using SolutionValue = typename SolutionManager::SolutionValue;
    using MoveValue = typename NeighborhoodExplorer::MoveValue;
    
    HillClimbing(std::shared_ptr<const SolutionManager> sm, std::shared_ptr<const NeighborhoodExplorer> ne, size_t random_seed) : Runner<SolutionManager, NeighborhoodExplorer>(sm, ne), random_seed(random_seed) {}
    
    void SetParameters(po::variables_map& vm, std::vector<std::string> to_pass_further) override
    {
        po::options_description desc("Set of parameters associated with the required HC.");
        termination.add_parameter(desc);
        select_move.add_parameter(desc);
        accept_move.add_parameter(desc);
        po::store(po::command_line_parser(to_pass_further).options(desc).run(), vm);
        po::notify(vm);
    }
    
protected:

    virtual void Go(std::shared_ptr<const Input> in) override
    {
        rng.seed(random_seed);
        PrintParameters();
        current_solution_value = std::make_shared<SolutionValue>(this->sm->CreateSolutionValue(this->sm->InitialSolution(in)));
        
        while (!termination.terminate(this) && !this->StopRun())
        {
            try
            {
                current_move_value = std::make_shared<MoveValue>(select_move.select(this));
            }
            catch (EmptyNeighborhood)
            {
#if !defined(NDEBUG)
                spdlog::debug("empty neighborhood encountered while exploring");
#endif
                break;
            }
            
            if (accept_move.accept(this))
            {
                // make move
                *current_solution_value = *current_move_value;
                idle_iteration = 0;
                std::ostringstream oss;
                oss << (*(current_solution_value->GetSolution()));
                // std::cout << oss.str() << std::endl;
                spdlog::info("{} --> {}", oss.str(), current_solution_value->AggregatedCost());
            }
            else
            {
                idle_iteration = idle_iteration + 1;
            }
            iteration = iteration + 1;
        }
        
        // post processings
        /*spdlog::info("Post processing after {} iterations (idle: {})", iteration, idle_iteration);
        auto values = current_solution_value->GetValues();
        std::ostringstream oss;
        oss << (*(current_solution_value->GetSolution()));
        spdlog::info("{} ---> ({})", oss.str(), spdlog::fmt_lib::join(values, ", "));
*/
        assert(current_solution_value->CheckValues());
        
        this->final_solution_value = std::make_shared<SolutionValue>(*current_solution_value);
    }
    
public:
    // object data
    size_t iteration = 0, idle_iteration = 0;
    std::shared_ptr<SolutionValue> current_solution_value;
    std::shared_ptr<MoveValue> current_move_value;    
protected:
    void PrintParameters()
    {
        termination.print_parameters();
    }
    // parametrized criteria
    TerminationCriterion<SelfClass> termination;
    SelectMove<SelfClass> select_move;
    AcceptMove<SelfClass> accept_move;
    mutable std::mt19937_64 rng;
    size_t random_seed;
};
}


// end --- hill-climbing.hh --- 



// begin --- neighborhood-explorer.hh --- 

////
////  neighborhood-explorer.hh
////  poc
////
////  Created by Luca Di Gaspero on 09/03/23.
////
//
#pragma once
#include <exception>

namespace easylocal {

class EmptyNeighborhood : public std::exception
{};

  // TODO: add the proper concepts for solution manager
  // TODO: the last template parameter is the neighborhood explorer itself, to be used in a CRTP (Curiously Recurring Template Pattern) for providing the make_move method below in a static fashion (therefore without overhead) in C++23 there will be P0847 feature (deducing this) that will allow to get rid of it
  template <SolutionManagerT _SolutionManager, class _Move, class SelfClass>
class NeighborhoodExplorer : public std::enable_shared_from_this<SelfClass>
  {
  public:
    using SolutionManager = _SolutionManager ;
    using Input = typename SolutionManager::Input;
    using Solution = typename SolutionManager::Solution;
    using T = typename SolutionManager::T;
    using Move = _Move;
    using CostStructure = typename SolutionManager::CostStructure;
    friend class MoveValue<Input, Solution, T, CostStructure, SelfClass>;
    using MoveValue = MoveValue<Input, Solution, T, CostStructure, SelfClass>;
    using SolutionValue = SolutionValue<Input, Solution, T, CostStructure>;
    using ThisClass = NeighborhoodExplorer<SolutionManager, Move, SelfClass>;

    NeighborhoodExplorer(std::shared_ptr<const SolutionManager> sm) noexcept
    {
      delta_cost_components.resize(sm->Components());
    }
    
    MoveValue CreateMoveValue(const SolutionValue& sv, const Move& mv) const
    {
      auto self = this->shared_from_this();
      return { self, sv, mv, sv.size() };
    }
    
    template <DeltaCostComponentT<Input, Solution, T, Move> DeltaCostComponent>
    void AddDeltaCostComponent(DeltaCostComponent& dcc, size_t i)
    {
      delta_cost_components[i] = std::make_unique<DeltaCostComponent>(dcc);
    }
    
//  protected:
    
    bool HasDeltaCostComponent(size_t i, const Move&) const
    {
      return delta_cost_components[i] != nullptr;
    }
    
    T ComputeDeltaCost(std::shared_ptr<const Solution> sol, const Move& mv, size_t i) const
    {
        assert(delta_cost_components[i] != nullptr);
        return this->delta_cost_components[i]->ComputeDeltaCost(sol, mv);
    }

    std::vector<std::unique_ptr<DeltaCostComponent<Input, Solution, T, Move>>> delta_cost_components;
  };
}


// end --- neighborhood-explorer.hh --- 

