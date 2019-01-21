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
    template <class StateManager, class NeighborhoodExplorer>
    class SimulatedAnnealing : public AbstractSimulatedAnnealing<StateManager, NeighborhoodExplorer>
    {
    public:
      UNPACK_MOVERUNNER_BASIC_TYPES()
      
      using AbstractSimulatedAnnealing<StateManager, NeighborhoodExplorer>::AbstractSimulatedAnnealing;
      
      ENABLE_RUNNER_CLONE()
    protected:
      void InitializeParameters() override
      {
        AbstractSimulatedAnnealing<StateManager, NeighborhoodExplorer>::InitializeParameters();
        min_temperature("min_temperature", "Minimum temperature", this->parameters);

      }
      
      /**
       Initializes the run by invoking the companion superclass method, and
       setting the temperature to the start value.
       */
      void InitializeRun(const Input& in) override
      {
        if (min_temperature <= 0.0)
        {
          throw IncorrectParameterValue(min_temperature, "should be greater than zero");
        }
        AbstractSimulatedAnnealing<StateManager, NeighborhoodExplorer>::InitializeRun(in);
      }
      
      /**
       The search stops when a low temperature has reached.
       */
      bool StopCriterion() const override
      {
        return this->temperature <= min_temperature;
      }
      
      // parameters
      Parameter<double> min_temperature;
    };
  } // namespace Core
} // namespace EasyLocal
