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
    template <class Input, class State, class Move, class CostStructure = DefaultCostStructure<int>>
    class SimulatedAnnealing : public AbstractSimulatedAnnealing<Input, State, Move, CostStructure>
    {
    public:
      using AbstractSimulatedAnnealing<Input, State, Move, CostStructure>::AbstractSimulatedAnnealing;
      
      std::string StatusString() const;
      
    protected:
      void InitializeParameters();
      void InitializeRun();
      bool StopCriterion();
      // parameters
      Parameter<double> min_temperature;
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    template <class Input, class State, class Move, class CostStructure>
    void SimulatedAnnealing<Input, State, Move, CostStructure>::InitializeParameters()
    {
      AbstractSimulatedAnnealing<Input, State, Move, CostStructure>::InitializeParameters();
      min_temperature("min_temperature", "Minimum temperature", this->parameters);
    }
    
    /**
     Initializes the run by invoking the companion superclass method, and
     setting the temperature to the start value.
     */
    template <class Input, class State, class Move, class CostStructure>
    void SimulatedAnnealing<Input, State, Move, CostStructure>::InitializeRun()
    {
      if (min_temperature <= 0.0)
      {
        throw IncorrectParameterValue(min_temperature, "should be greater than zero");
      }
      AbstractSimulatedAnnealing<Input, State, Move, CostStructure>::InitializeRun();
    }
    
    /**
     The search stops when a low temperature has reached.
     */
    template <class Input, class State, class Move, class CostStructure>
    bool SimulatedAnnealing<Input, State, Move, CostStructure>::StopCriterion()
    {
      return this->temperature <= min_temperature;
    }
  } // namespace Core
} // namespace EasyLocal
