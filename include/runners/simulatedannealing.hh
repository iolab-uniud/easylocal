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
      std::unique_ptr<Runner<Input, State, CostStructure>> Clone() const override;
      
      std::string StatusString() const;
      
    protected:
      void InitializeParameters() override;
      void InitializeRun(const Input& in) override;
      bool StopCriterion() const override;
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
    void SimulatedAnnealing<Input, State, Move, CostStructure>::InitializeRun(const Input& in)
    {
      if (min_temperature <= 0.0)
      {
        throw IncorrectParameterValue(min_temperature, "should be greater than zero");
      }
      AbstractSimulatedAnnealing<Input, State, Move, CostStructure>::InitializeRun(in);
    }
    
    /**
     The search stops when a low temperature has reached.
     */
    template <class Input, class State, class Move, class CostStructure>
    bool SimulatedAnnealing<Input, State, Move, CostStructure>::StopCriterion() const
    {
      return this->temperature <= min_temperature;
    }
    
    template <class Input, class State, class Move, class CostStructure>
    std::unique_ptr<Runner<Input, State, CostStructure>> SimulatedAnnealing<Input, State, Move, CostStructure>::Clone() const
    {
      return Runner<Input, State, CostStructure>::MakeClone(this);
    }
  } // namespace Core
} // namespace EasyLocal
