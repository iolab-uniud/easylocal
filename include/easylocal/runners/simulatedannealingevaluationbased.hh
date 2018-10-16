#pragma once

#include "easylocal/runners/abstractsimulatedannealing.hh"

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
    
    template <class Input, class State, class Move, class CostStructure = DefaultCostStructure<int>>
    class SimulatedAnnealingEvaluationBased : public AbstractSimulatedAnnealing<Input, State, Move, CostStructure>
    {
    public:
      using AbstractSimulatedAnnealing<Input, State, Move, CostStructure>::AbstractSimulatedAnnealing;
      std::unique_ptr<Runner<Input, State, CostStructure>> Clone() const override;
      
    protected:
      void InitializeParameters() override;
      void InitializeRun(const Input& in) override;
      bool StopCriterion() const override;
      
      // additional parameters
      Parameter<double> neighbors_accepted_ratio;
      Parameter<double> temperature_range;
      Parameter<double> expected_min_temperature;
      unsigned int expected_number_of_temperatures;
      //      double expected_min_temperature;
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    template <class Input, class State, class Move, class CostStructure>
    void SimulatedAnnealingEvaluationBased<Input, State, Move, CostStructure>::InitializeParameters()
    {
      AbstractSimulatedAnnealing<Input, State, Move, CostStructure>::InitializeParameters();
      neighbors_accepted_ratio("neighbors_accepted_ratio", "Ratio of neighbors accepted", this->parameters);
      temperature_range("temperature_range", "Temperature_range", this->parameters);
      expected_min_temperature("expected_min_temperature", "Expected minimum temperature", this->parameters);
      this->max_neighbors_sampled = this->max_neighbors_accepted = 0;
    }
    
    /**
     Initializes the run by invoking the companion superclass method, and
     setting the temperature to the start value.
     */
    template <class Input, class State, class Move, class CostStructure>
    void SimulatedAnnealingEvaluationBased<Input, State, Move, CostStructure>::InitializeRun(const Input& in)
    {
      AbstractSimulatedAnnealing<Input, State, Move, CostStructure>::InitializeRun(in);
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
    
    /**
     The search stops when the number of evaluations is expired (already checked in the superclass MoveRunner)
     */
    template <class Input, class State, class Move, class CostStructure>
    bool SimulatedAnnealingEvaluationBased<Input, State, Move, CostStructure>::StopCriterion() const
    {
      return false;
    }
  
    template <class Input, class State, class Move, class CostStructure>
    std::unique_ptr<Runner<Input, State, CostStructure>> SimulatedAnnealingEvaluationBased<Input, State, Move, CostStructure>::Clone() const
    {
      return std::make_unique<SimulatedAnnealingEvaluationBased<Input, State, Move, CostStructure>>(*this);
    }
  } // namespace Core
} // namespace EasyLocal
