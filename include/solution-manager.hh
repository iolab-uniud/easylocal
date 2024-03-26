//
//  solution-manager.hh
//  poc
//
//  Created by Luca Di Gaspero on 09/03/23.
//

#pragma once

#include "concepts.hh"
#include "cost-components.hh"

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
