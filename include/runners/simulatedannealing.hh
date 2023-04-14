#pragma once

#include "runners/abstractsimulatedannealing.hh"

namespace EasyLocal
{
  
  namespace Core
  {
    
    /** Implements the Simulated annealing runner with a stop condition
     based on the minimum temperature.
     
     @ingroup Runners
     */
    template <class Input, class Solution, class Move, class CostStructure = DefaultCostStructure<int>>
    class SimulatedAnnealing : public AbstractSimulatedAnnealing<Input, Solution, Move, CostStructure>
    {
    public:
      SimulatedAnnealing(const Input &in, SolutionManager<Input, Solution, CostStructure> &sm,
                         NeighborhoodExplorer<Input, Solution, Move, CostStructure> &ne,
                         std::string name) : AbstractSimulatedAnnealing<Input, Solution, Move, CostStructure>(in, sm, ne, name)
      
      std::string StatusString() const;
      
    protected:
      void InitializeRun();
      bool StopCriterion();
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    /**
     Initializes the run by invoking the companion superclass method, and
     setting the temperature to the start value.
     */
    template <class Input, class Solution, class Move, class CostStructure>
    void SimulatedAnnealing<Input, Solution, Move, CostStructure>::InitializeRun()
    {
      if (this->min_temperature <= 0.0)
      {
        throw IncorrectParameterValue(this->min_temperature, "should be greater than zero");
      }
      AbstractSimulatedAnnealing<Input, Solution, Move, CostStructure>::InitializeRun();
    }
    
    /**
     The search stops when a low temperature has reached.
     */
    template <class Input, class Solution, class Move, class CostStructure>
    bool SimulatedAnnealing<Input, Solution, Move, CostStructure>::StopCriterion()
    {
      return this->temperature <= this->min_temperature;
    }
  } // namespace Core
} // namespace EasyLocal
