#pragma once

#include "runners/simulatedannealing.hh"
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
    class SimulatedAnnealingTimeBased : public SimulatedAnnealing<Input, Solution, Move, CostStructure>
    {
    public:
        SimulatedAnnealingTimeBased(const Input &in, SolutionManager<Input, Solution, CostStructure> &sm,
                                    NeighborhoodExplorer<Input, Solution, Move, CostStructure> &ne,
                                    std::string name) : SimulatedAnnealing<Input, Solution, Move, CostStructure>(in, sm, ne, name)
        {
            allowed_running_time("allowed_running_time", "Allowed running time", this->parameters);
        }
      
    protected:
      void InitializeRun() override;
      bool StopCriterion() override;
      //      void CompleteIteration() override;
      bool MaxEvaluationsExpired() const override;
      bool CoolingNeeded() const override;
      void ApplyCooling() override;
      void PrintStatus(ostream& os) const override;

      // additional parameters
      Parameter<double> allowed_running_time;
      std::chrono::time_point<std::chrono::system_clock> run_start, temperature_start_time, temperature_end_time;
      std::chrono::milliseconds run_duration, residual_running_time, allowed_running_time_per_temperature;
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    /**
     Initializes the run by invoking the companion superclass method, and
     setting the temperature to the start value.
     */
    template <class Input, class Solution, class Move, class CostStructure>
    void SimulatedAnnealingTimeBased<Input, Solution, Move, CostStructure>::InitializeRun()
    {
      if (!this->max_evaluations.IsSet()) // needed set by InitializeRun of upper class SimulatedAnnealing
        this->max_evaluations = std::numeric_limits<unsigned long int>::max();
      SimulatedAnnealing<Input, Solution, Move, CostStructure>::InitializeRun();

      run_duration = std::chrono::milliseconds(static_cast<int>(1000.0 * allowed_running_time));
      allowed_running_time_per_temperature = run_duration / this->total_number_of_temperatures;
      run_start = std::chrono::system_clock::now();
      temperature_start_time = run_start;
    }
    
    /**
     The search stops when the number of evaluations is expired (already checked in the superclass MoveRunner) or the duration of the run is above the allowed one.
     */
    template <class Input, class Solution, class Move, class CostStructure>
    bool SimulatedAnnealingTimeBased<Input, Solution, Move, CostStructure>::StopCriterion()
    {
      return std::chrono::system_clock::now() > run_start + run_duration 
        || SimulatedAnnealing<Input, Solution, Move, CostStructure>::StopCriterion();
    }
    
    template <class Input, class Solution, class Move, class CostStructure>
    void SimulatedAnnealingTimeBased<Input, Solution, Move, CostStructure>::ApplyCooling()
    {
      SimulatedAnnealing<Input, Solution, Move, CostStructure>::ApplyCooling();
      temperature_end_time = std::chrono::system_clock::now();
      
      if (temperature_end_time - temperature_start_time < allowed_running_time_per_temperature && this->residual_temperatures > 0)
        {
          residual_running_time = chrono::duration_cast<chrono::milliseconds>(run_duration - (temperature_end_time - run_start));
          allowed_running_time_per_temperature = residual_running_time/this->residual_temperatures;
        }
      temperature_start_time = temperature_end_time;
    }

    template <class Input, class Solution, class Move, class CostStructure>
    void SimulatedAnnealingTimeBased<Input, Solution, Move, CostStructure>::PrintStatus(ostream& os) const
    {
      SimulatedAnnealing<Input, Solution, Move, CostStructure>::PrintStatus(os);
      os << ", t = " << chrono::duration_cast<chrono::milliseconds>(temperature_start_time - run_start).count()/1000.0;
    }

  template <class Input, class Solution, class Move, class CostStructure>
    bool SimulatedAnnealingTimeBased<Input, Solution, Move, CostStructure>::CoolingNeeded() const
    {
      // In this version of SA (TimeBased)temperature is decreased based on running
      // time or cut-off (no cooling based on number of iterations)
      return std::chrono::system_clock::now() > temperature_start_time + allowed_running_time_per_temperature 
          || SimulatedAnnealing<Input, Solution, Move, CostStructure>::CoolingNeeded();
    }

    template <class Input, class Solution, class Move, class CostStructure>
    bool SimulatedAnnealingTimeBased<Input, Solution, Move, CostStructure>::MaxEvaluationsExpired() const
    {
      return false;
    }
  } // namespace Core

} // namespace EasyLocal
