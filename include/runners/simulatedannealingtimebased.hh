#pragma once

#include "runners/abstractsimulatedannealing.hh"
#include <chrono>

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
    
    template <class Input, class Solution, class Move, class CostStructure = DefaultCostStructure<int>>
    class SimulatedAnnealingTimeBased : public AbstractSimulatedAnnealing<Input, Solution, Move, CostStructure>
    {
    public:
      using AbstractSimulatedAnnealing<Input, Solution, Move, CostStructure>::AbstractSimulatedAnnealing;
      
    protected:
      void InitializeParameters() override;
      void InitializeRun() override;
      bool StopCriterion() override;
      void CompleteIteration() override;
      bool MaxEvaluationsExpired() const override;
      
      // additional parameters
      Parameter<double> neighbors_accepted_ratio;
      Parameter<double> temperature_range;
      Parameter<double> expected_min_temperature;
      unsigned int expected_number_of_temperatures;
      Parameter<double> allowed_running_time;
      std::chrono::time_point<std::chrono::system_clock> run_start, temperature_start_time;
      std::chrono::milliseconds time_cutoff, run_duration,allowed_running_time_per_temperature;
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    template <class Input, class Solution, class Move, class CostStructure>
    void SimulatedAnnealingTimeBased<Input, Solution, Move, CostStructure>::InitializeParameters()
    {
      AbstractSimulatedAnnealing<Input, Solution, Move, CostStructure>::InitializeParameters();
      neighbors_accepted_ratio("neighbors_accepted_ratio", "Ratio of neighbors accepted", this->parameters);
      temperature_range("temperature_range", "Temperature range", this->parameters);
      expected_min_temperature("expected_min_temperature", "Expected minimum temperature", this->parameters);
      allowed_running_time("allowed_running_time", "Allowed running time", this->parameters);
      this->max_neighbors_sampled = this->max_neighbors_accepted = 0;
    }
    
    /**
     Initializes the run by invoking the companion superclass method, and
     setting the temperature to the start value.
     */
    template <class Input, class Solution, class Move, class CostStructure>
    void SimulatedAnnealingTimeBased<Input, Solution, Move, CostStructure>::InitializeRun()
    {
      AbstractSimulatedAnnealing<Input, Solution, Move, CostStructure>::InitializeRun();
      if (temperature_range.IsSet())
        expected_min_temperature = this->start_temperature / temperature_range;
      else
        temperature_range = this->start_temperature / expected_min_temperature;
      
      expected_number_of_temperatures = static_cast<unsigned int>(ceil(-log(temperature_range) / log(this->cooling_rate)));
      
      this->max_neighbors_sampled = static_cast<unsigned int>(this->max_evaluations / expected_number_of_temperatures);
     
      // If the ratio of accepted neighbors for each temperature is not set,
      // FIXME: in future versions, the ratio should be definitely removed
      if (!neighbors_accepted_ratio.IsSet())
        this->max_neighbors_accepted = this->max_neighbors_sampled;
      else
        this->max_neighbors_accepted = static_cast<unsigned int>(this->max_neighbors_sampled * neighbors_accepted_ratio);
      run_duration = std::chrono::seconds(allowed_running_time);
      allowed_running_time_per_temperature = run_duration / expected_number_of_temperatures;
      time_cutoff = run_duration / expected_number_of_temperatures;
      run_start = std::chrono::system_clock::now();
      temperature_start_time = run_start;
    }
    
    /**
     The search stops when the number of evaluations is expired (already checked in the superclass MoveRunner) or the duration of the run is above the allowed one.
     */
    template <class Input, class Solution, class Move, class CostStructure>
    bool SimulatedAnnealingTimeBased<Input, Solution, Move, CostStructure>::StopCriterion()
    {
      return std::chrono::system_clock::now() > run_start + run_duration;
    }
    
    template <class Input, class Solution, class Move, class CostStructure>
    void SimulatedAnnealingTimeBased<Input, Solution, Move, CostStructure>::CompleteIteration()
    {
      // In this version of SA (TimeBased)temperature is decreased based on running
      // time or cut-off (no cooling based on number of iterations)
      if (std::chrono::system_clock::now() > temperature_start_time + allowed_running_time_per_temperature 
          || this->neighbors_accepted >= this->max_neighbors_accepted
          || this->neighbors_sampled >= this->max_neighbors_sampled)
      {
//         char ch;
//         if (this->neighbors_accepted >= this->max_neighbors_accepted) 
//           ch = 'A';
//         else if (this->neighbors_sampled >= this->max_neighbors_sampled) 
//           ch = 'S';
//         else
//           ch = 'T';           
        this->temperature *= this->cooling_rate;
//         cerr << ch << this->temperature << " ";
        this->number_of_temperatures++;
        this->neighbors_sampled = 0;
        this->neighbors_accepted = 0;
        temperature_start_time = std::chrono::system_clock::now();
      }
    }
    
    template <class Input, class Solution, class Move, class CostStructure>
    bool SimulatedAnnealingTimeBased<Input, Solution, Move, CostStructure>::MaxEvaluationsExpired() const
    {
      return false;
    }
  } // namespace Core

} // namespace EasyLocal
