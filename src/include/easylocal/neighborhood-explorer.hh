////
////  neighborhood-explorer.hh
////  poc
////
////  Created by Luca Di Gaspero on 09/03/23.
////
//
#pragma once

#include "concepts.hh"
#include "cost-components.hh"
#include "utils.hh"
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
