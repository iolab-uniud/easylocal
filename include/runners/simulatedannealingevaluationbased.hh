#pragma once

#include "runners/abstractsimulatedannealing.hh"

namespace EasyLocal
{
  namespace Core
  {
    /** Implements the Simulated annealing runner with a stop condition
     based on the number of iterations. In addition, the number of neighbors
     sampled at each iteration is computed in such a way that the total number
     of evaluations is fixed
     
     @ingroup Runners
     */
    template <class StateManager, class NeighborhoodExplorer>
    class SimulatedAnnealingEvaluationBased : public AbstractSimulatedAnnealing<StateManager, NeighborhoodExplorer>
    {
    public:
      UNPACK_MOVERUNNER_BASIC_TYPES()
      
      using AbstractSimulatedAnnealing<StateManager, NeighborhoodExplorer>::AbstractSimulatedAnnealing;

      ENABLE_RUNNER_CLONE()
      
    protected:
      void InitializeParameters() override
      {
        AbstractSimulatedAnnealing<StateManager, NeighborhoodExplorer>::InitializeParameters();
        neighbors_accepted_ratio("neighbors_accepted_ratio", "Ratio of neighbors accepted", this->parameters);
        temperature_range("temperature_range", "Temperature_range", this->parameters);
        expected_min_temperature("expected_min_temperature", "Expected minimum temperature", this->parameters);
        this->max_neighbors_sampled = this->max_neighbors_accepted = 0;
      }
      
      void InitializeRun(const Input& in) override
      {
        AbstractSimulatedAnnealing<StateManager, NeighborhoodExplorer>::InitializeRun(in);
        if (temperature_range.IsSet())
          expected_min_temperature = this->start_temperature / temperature_range;
        else
          temperature_range = this->start_temperature / expected_min_temperature;
        
        expected_number_of_temperatures = static_cast<unsigned int>(ceil(-log(temperature_range) / log(this->cooling_rate)));
        
        this->max_neighbors_sampled = static_cast<unsigned int>(this->max_evaluations / expected_number_of_temperatures);
        
        // If the ratio of accepted neighbors for each temperature is not set,
        if (!neighbors_accepted_ratio.IsSet())
          this->max_neighbors_accepted = this->max_neighbors_sampled;
        else
          this->max_neighbors_accepted = static_cast<unsigned int>(this->max_neighbors_sampled * neighbors_accepted_ratio);
      }
      
      bool StopCriterion() const override
      {
        return false;
      }
      
      // additional parameters
      Parameter<double> neighbors_accepted_ratio;
      Parameter<double> temperature_range;
      Parameter<double> expected_min_temperature;
      unsigned int expected_number_of_temperatures;
    };
  } // namespace Core
} // namespace EasyLocal
