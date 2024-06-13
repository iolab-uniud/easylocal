//
//  multi-modal-neighborhood-explorer.hh
//  poc
//
//  Created by Luca Di Gaspero on 09/03/23.
//

#pragma once

#include "concepts.hh"
#include "cost-components.hh"
#include <random>
#include <variant>
#include "utils.hh"

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
