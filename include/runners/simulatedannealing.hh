#pragma once

#include <cmath>
#include <stdexcept>
#include <algorithm>

#include "runners/moverunner.hh"
#include "helpers/solutionmanager.hh"
#include "helpers/neighborhoodexplorer.hh"

namespace EasyLocal
{
namespace Core
{

/** The  Simulated annealing runner relies on a probabilistic local
 search technique whose name comes from the fact that it
 simulates the cooling of a collection of hot vibrating atoms.
 
 At each iteration a candidate move is generated at random, and
 it is always accepted if it is an improving move.  Instead, if
 the move is a worsening one, the new solution is accepted with
 time decreasing probability.
 
 The stop condition is delegated to the concrete subclasses
 
 @ingroup Runners
 */
template <class Input, class Solution, class Move, class CostStructure = DefaultCostStructure<int>>
class SimulatedAnnealing : public MoveRunner<Input, Solution, Move, CostStructure>
{
public:
  double Temperature() const { return temperature; }
  
  SimulatedAnnealing(const Input &in, SolutionManager<Input, Solution, CostStructure> &e_sm,
                     NeighborhoodExplorer<Input, Solution, Move, CostStructure> &e_ne,
                     std::string name);
protected:
  void InitializeRun() override;
  void UpdateIterationCounter();
  void SelectMove() override;
  void CompleteMove() override;
  void CompleteIteration() override;
  bool MetropolisCriterion() const;
  virtual bool CoolingNeeded() const; 
  virtual void ApplyCooling();
  bool StopCriterion() override;
  void ComputeStartTemperature();
  virtual void PrintStatus(std::ostream& os) const;
  // parameters
  Parameter<bool> compute_start_temperature;
  Parameter<double> start_temperature, min_temperature;
  Parameter<double> neighbors_accepted_ratio;
  Parameter<double> cooling_rate;
  Parameter<unsigned int> max_neighbors_sampled;
  Parameter<unsigned int> max_neighbors_accepted;  
  // state of SA
  double temperature; /**< The current temperature. */
  unsigned int current_max_neighbors_sampled; // initially set to the max value, recomputed based on saved iterations by cut-off
  
  size_t neighbors_sampled, neighbors_accepted;  
  unsigned residual_temperatures, residual_iterations;
  int number_of_temperatures;
  int total_number_of_temperatures;
  double temperature_range;
};
  
  /*************************************************************************
 * Implementation
 *************************************************************************/

/** Constructor

*/
template <class Input, class Solution, class Move, class CostStructure>
SimulatedAnnealing<Input, Solution, Move, CostStructure>::SimulatedAnnealing(const Input &in, SolutionManager<Input, Solution, CostStructure> &e_sm,
                                       NeighborhoodExplorer<Input, Solution, Move, CostStructure> &e_ne,
                                       std::string name) 
  : MoveRunner<Input, Solution, Move, CostStructure>(in, e_sm, e_ne, name)
{
  compute_start_temperature("compute_start_temperature", "Should the runner compute the initial temperature?", this->parameters);
  start_temperature("start_temperature", "Starting temperature", this->parameters);
  min_temperature("min_temperature", "Final temperature", this->parameters);
  cooling_rate("cooling_rate", "Cooling rate", this->parameters);
  max_neighbors_sampled("max_neighbors_sampled", "Maximum number of neighbors sampled at each temp.", this->parameters);
  max_neighbors_accepted("max_neighbors_accepted", "Maximum number of neighbors accepted at each temp.", this->parameters);
  neighbors_accepted_ratio("neighbors_accepted_ratio", "Ratio of neighbors accepted", this->parameters);
}

/**
 Initializes the run by invoking the companion superclass method, and
 setting the temperature to the start value.
 */
template <class Input, class Solution, class Move, class CostStructure>
void SimulatedAnnealing<Input, Solution, Move, CostStructure>::InitializeRun()
{
  MoveRunner<Input, Solution, Move, CostStructure>::InitializeRun();

  if (!compute_start_temperature.IsSet())
    compute_start_temperature = false; 

  if (!neighbors_accepted_ratio.IsSet() && !max_neighbors_accepted.IsSet())
     neighbors_accepted_ratio = 1.0;  
  
  if (cooling_rate <= 0.0 || cooling_rate >= 1.0)
    throw IncorrectParameterValue(cooling_rate, "should be a value in the interval ]0, 1[");
    
  if (max_neighbors_sampled.IsSet() && this->max_evaluations.IsSet()) 
    throw IncorrectParameterValue(max_neighbors_sampled, "should not be set explicitly when max_evaluations is set explicitly, as it is computed");

  if (max_neighbors_accepted.IsSet() && neighbors_accepted_ratio.IsSet()) 
    throw IncorrectParameterValue(max_neighbors_accepted, "should not be set explicitly when neighbors_accepted_ratio is set explicitly, as it is computed");

  if (!max_neighbors_sampled.IsSet() && !this->max_evaluations.IsSet()) 
    throw IncorrectParameterValue(max_neighbors_sampled, "should be set if max_evaluations is not set explicitly");

  if (compute_start_temperature)
    {
      if (start_temperature.IsSet())
         throw IncorrectParameterValue(start_temperature, "should not be assigned, as it is computed");
      ComputeStartTemperature();
     }

  if (start_temperature < min_temperature)
    throw IncorrectParameterValue(start_temperature, "should be greater than min_temperature");
  if (min_temperature <= 0.0)
    throw IncorrectParameterValue(min_temperature, "should be greater than zero");

  temperature = start_temperature;
  temperature_range = start_temperature / min_temperature;
  total_number_of_temperatures = static_cast<unsigned>(ceil(-log(temperature_range) / log(cooling_rate)));      
  if (this->max_evaluations.IsSet())
    { // Compute max_neighbors_sampled from max_evaluations
      max_neighbors_sampled = static_cast<unsigned>(this->max_evaluations / total_number_of_temperatures);
    }

  // max_neighbors_sampled is fixed (and used for cut-off), its current value changes due to saved iterations
  current_max_neighbors_sampled = max_neighbors_sampled;

  if (!max_neighbors_accepted.IsSet())
    max_neighbors_accepted = static_cast<unsigned>(max_neighbors_sampled * neighbors_accepted_ratio);
    
  // initialize dynamic counters
  neighbors_sampled = 0;
  neighbors_accepted = 0;
  number_of_temperatures = 1;
}

template <class Input, class Solution, class Move, class CostStructure>
void SimulatedAnnealing<Input, Solution, Move, CostStructure>::ComputeStartTemperature()
{
  // TODO: test this procedure
  // Compute a start temperature by sampling the search space and computing the variance
  // according to [van Laarhoven and Aarts, 1987] (allow an acceptance ratio of approximately 80%)
  const unsigned int samples = 100;
  std::vector<CostStructure> cost_values(samples);
  double mean = 0.0, variance = 0.0;
  for (unsigned int i = 0; i < samples; i++)
    {
      Move mv;
      this->ne.RandomMove(*this->p_current_state, mv);
      cost_values[i] = this->ne.DeltaCostFunctionComponents(*this->p_current_state, mv);
      mean += cost_values[i].total;
    }
  mean /= samples;
  for (unsigned int i = 0; i < samples; i++)
    variance += (cost_values[i].total - mean) * (cost_values[i].total - mean) / samples;
  start_temperature = variance;
  /*From "An improved annealing scheme for the QAP. Connoly. EJOR 46 (1990) 93-100"
    temperature = min(cost_values.begin(), cost_values.end()) + (max(cost_values.begin(), cost_values.end()) - min(cost_values.begin(), cost_values.end()))/10;*/
}

/**
 A move is randomly picked.
 */
template <class Input, class Solution, class Move, class CostStructure>
void SimulatedAnnealing<Input, Solution, Move, CostStructure>::SelectMove()
{
  this->ne.RandomMove(*this->p_current_state, this->current_move.move);
  this->current_move.cost = this->ne.DeltaCostFunctionComponents(*this->p_current_state, this->current_move.move);
#if VERBOSE >= 3
  std::cerr << "V3 " << this->current_move.move << " (" << this->current_move.cost << ") ";
  PrintStatus(std::cerr);
  std::cerr << std::endl;
#endif
  this->neighbors_sampled++;
  this->evaluations++;
  this->current_move.is_valid = this->current_move.cost <= 0 || MetropolisCriterion();
}

template <class Input, class Solution, class Move, class CostStructure>
void SimulatedAnnealing<Input, Solution, Move, CostStructure>::PrintStatus(std::ostream& os) const
{
  os << "Status: (" << this->number_of_temperatures << "|" << this->evaluations << ")" 
     << " T = " << this->Temperature() 
     << " S/A/ar = [" << this->neighbors_sampled << "/" << this->current_max_neighbors_sampled << "|" 
     << this->neighbors_accepted << "/" << this->max_neighbors_accepted << "|" 
     << static_cast<double>(this->neighbors_accepted)/this->neighbors_sampled << "],"
     << " OF = [" << this->current_state_cost.total << "/" << this->best_state_cost.total << "]";
}

/**
 SA acceptance criterion
 */
template <class Input, class Solution, class Move, class CostStructure>
bool SimulatedAnnealing<Input, Solution, Move, CostStructure>::MetropolisCriterion() const
{
  return this->current_move.cost < (-this->temperature * log(std::max(Random::Uniform<double>(0.0, 1.0), std::numeric_limits<double>::epsilon())));
}

/**

 */
template <class Input, class Solution, class Move, class CostStructure>
void SimulatedAnnealing<Input, Solution, Move, CostStructure>::CompleteMove()
{
  neighbors_accepted++;
#if VERBOSE >= 2
      std::cerr << "V2 " << this->current_move.move << " (" << this->current_move.cost << ") ";
      PrintStatus(std::cerr);
      std::cerr << std::endl;
#endif
}

/**
 At regular steps, the temperature is decreased
 multiplying it by a cooling rate.
 */
template <class Input, class Solution, class Move, class CostStructure>
void SimulatedAnnealing<Input, Solution, Move, CostStructure>::CompleteIteration()
{
  if (CoolingNeeded())
    ApplyCooling();
}

template <class Input, class Solution, class Move, class CostStructure>
bool SimulatedAnnealing<Input, Solution, Move, CostStructure>::CoolingNeeded() const
{
  return neighbors_sampled >= current_max_neighbors_sampled || neighbors_accepted >= max_neighbors_accepted;
} 

template <class Input, class Solution, class Move, class CostStructure>
void SimulatedAnnealing<Input, Solution, Move, CostStructure>::ApplyCooling()
{
  residual_temperatures = total_number_of_temperatures - number_of_temperatures; 
  if (neighbors_sampled < current_max_neighbors_sampled && residual_temperatures > 0) 
    { // we have saved some iterations thanks to the cut-off: they are 
      // redistributed to the remaining temperatures
      residual_iterations = this->max_evaluations - this->evaluations;
      current_max_neighbors_sampled = residual_iterations/residual_temperatures;
      // NOTE: the number of accepted moves depends on the initial number of sampled, NOT from the current one
    }
#if VERBOSE >= 1
  std::cerr << "V1 ";
  PrintStatus(std::cerr);
  std::cerr << std::endl;
#endif
  temperature *= cooling_rate;
  number_of_temperatures++;
  neighbors_sampled = 0;
  neighbors_accepted = 0;
} 


  /**
     The search stops when a low temperature has reached.
  */
  template <class Input, class Solution, class Move, class CostStructure>
  bool SimulatedAnnealing<Input, Solution, Move, CostStructure>::StopCriterion()
  {
    return temperature <= min_temperature;
  }
  
} // namespace Core
} // namespace EasyLocal
