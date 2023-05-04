#pragma once
#include "runners/simulatedannealing.hh"

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
    class SimulatedAnnealingEvaluationBased : public SimulatedAnnealing<Input, Solution, Move, CostStructure>
    {
    public:
      SimulatedAnnealingEvaluationBased(const Input &in, SolutionManager<Input, Solution, CostStructure> &sm,
                                        NeighborhoodExplorer<Input, Solution, Move, CostStructure> &ne,
                                        std::string name) : SimulatedAnnealing<Input, Solution, Move, CostStructure>(in, sm, ne, name)
      {  
        if (this->max_neighbors_sampled.IsSet()) 
          throw IncorrectParameterValue(this->max_neighbors_sampled, " should not be set explicitly, as it is computed");
        this->max_neighbors_sampled = 0; // computed later
      }
    protected:
      void InitializeRun() override;
      void CompleteIteration() override;
      
      // additional parameters

      // additional data
      unsigned int total_number_of_temperatures;
      double temperature_range;
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    /**
     Initializes the run by invoking the companion superclass method, and
     setting the temperature to the start value.
     */
    template <class Input, class Solution, class Move, class CostStructure>
    void SimulatedAnnealingEvaluationBased<Input, Solution, Move, CostStructure>::InitializeRun()
    {
      SimulatedAnnealing<Input, Solution, Move, CostStructure>::InitializeRun();

      temperature_range = this->start_temperature / this->min_temperature;
      total_number_of_temperatures = static_cast<unsigned int>(ceil(-log(temperature_range) / log(this->cooling_rate)));
      
      this->max_neighbors_sampled = static_cast<unsigned int>(this->max_evaluations / total_number_of_temperatures);
      this->current_max_neighbors_sampled = this->max_neighbors_sampled;

      // If the ratio of accepted neighbors for each temperature is not set,
      if (!this->neighbors_accepted_ratio.IsSet())
        this->max_neighbors_accepted = this->max_neighbors_sampled;
      else
        this->max_neighbors_accepted = static_cast<unsigned int>(this->max_neighbors_sampled * this->neighbors_accepted_ratio);
    }
    
    template <class Input, class Solution, class Move, class CostStructure>
    void SimulatedAnnealingEvaluationBased<Input, Solution, Move, CostStructure>::CompleteIteration()
    {
      if (this->CoolingNeeded())
        {
          unsigned residual_temperatures = total_number_of_temperatures - this->number_of_temperatures;          
          // additional operations, only for SimulatedAnnealingEvaluationBased
          if (this->neighbors_sampled < this->current_max_neighbors_sampled && residual_temperatures > 0) 
            { // we have a saving
              unsigned residual_iterations = this->max_evaluations - this->evaluations;
              this->current_max_neighbors_sampled = residual_iterations/residual_temperatures;
              this->max_neighbors_accepted = static_cast<unsigned int>(this->max_neighbors_sampled * this->neighbors_accepted_ratio);
              // NOTE: the number of accepted moves depends on the initial number of sampled, NOT from the current one
//               cerr << "Temp = " << this->temperature << ", saved iters = " << this->current_max_neighbors_sampled - this->neighbors_sampled << ", residual_temps = " << residual_temperatures 
//                    << ", residual_it = " << residual_iterations << ", N_s = " << this->current_max_neighbors_sampled << ", N_a = " 
//                    << this->max_neighbors_accepted << endl;
            }
//           else 
//             cerr << "Temp = " << this->temperature << endl;

          // same as father class SimulatedAnnealing
          this->temperature *= this->cooling_rate;
          this->number_of_temperatures++;
          this->neighbors_sampled = 0;
          this->neighbors_accepted = 0;
        }
    }
  } // namespace Core
} // namespace EasyLocal
