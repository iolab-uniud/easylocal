#pragma once

#include <cmath>
#include <algorithm>

#include "runners/moverunner.hh"

namespace EasyLocal
{
  namespace Core
  {
    /** The Abstract Simulated annealing runner relies on a probabilistic local
     search technique whose name comes from the fact that it
     simulates the cooling of a collection of hot vibrating atoms.
     
     At each iteration a candidate move is generated at random, and
     it is always accepted if it is an improving move.  Instead, if
     the move is a worsening one, the new solution is accepted with
     time decreasing probability.
     
     The stop condition is delegated to the concrete subclasses
     
     @ingroup Runners
     */
    template <class StateManager, class NeighborhoodExplorer>
    class AbstractSimulatedAnnealing : public MoveRunner<StateManager, NeighborhoodExplorer>
    {
    public:
      UNPACK_MOVERUNNER_BASIC_TYPES()
      
      using MoveRunner<StateManager, NeighborhoodExplorer>::MoveRunner;
            
    protected:
      /**
       Initializes the run by invoking the companion superclass method, and
       setting the temperature to the start value.
       */
      void InitializeRun(const Input& in) override
      {
        MoveRunner<StateManager, NeighborhoodExplorer>::InitializeRun(in);
        
        if (cooling_rate <= 0.0 || cooling_rate >= 1.0)
          throw IncorrectParameterValue(cooling_rate, "should be a value in the interval ]0, 1[");
        
        if (!compute_start_temperature)
        {
          if (start_temperature <= 0.0)
            throw IncorrectParameterValue(start_temperature, "should be greater than zero");
          temperature = start_temperature;
        }
        else
        {
          // Compute a start temperature by sampling the search space and computing the variance
          // according to [van Laarhoven and Aarts, 1987] (allow an acceptance ratio of approximately 80%)
          //State sampled_state(this->in);
          const unsigned int samples = 100;
          std::vector<CostStructure> cost_values(samples);
          double mean = 0.0, variance = 0.0;
          for (unsigned int i = 0; i < samples; i++)
          {
            //this->sm.RandomState(sampled_state);
            Move mv;
            this->ne.RandomMove(in, *this->p_current_state, mv);
            cost_values[i] = this->ne.DeltaCostFunctionComponents(in, *this->p_current_state, mv);
            mean += cost_values[i].total;
          }
          mean /= samples;
          for (unsigned int i = 0; i < samples; i++)
            variance += (cost_values[i].total - mean) * (cost_values[i].total - mean) / samples;
          temperature = variance;
          /*From "An improved annealing scheme for the QAP. Connoly. EJOR 46 (1990) 93-100"
           temperature = min(cost_values.begin(), cost_values.end()) + (max(cost_values.begin(), cost_values.end()) - min(cost_values.begin(), cost_values.end()))/10;*/
        }
        
        // If the number of maximum accepted neighbors for each temperature is not set, default to all of them
        if (!max_neighbors_accepted.IsSet())
          max_neighbors_accepted = max_neighbors_sampled;
        
        neighbors_sampled = 0;
        neighbors_accepted = 0;
      }
      
      void SelectMove(const Input& in) override
      {
        // TODO: it should become a parameter, the number of neighbors drawn at each iteration (possibly evaluated in parallel)
        size_t sampled;
        double t = this->temperature;
        EvaluatedMove em = this->ne.RandomFirst(in, *this->p_current_state, this->max_neighbors_sampled - neighbors_sampled, sampled, [t](const Move &mv, const CostStructure &move_cost) {
          double r = std::max(Random::Uniform<double>(0.0, 1.0), std::numeric_limits<double>::epsilon());
          return move_cost <= 0 || move_cost < (-t * log(r));
        },
                                                this->weights);
        this->current_move = em;
        neighbors_sampled += sampled;
        this->evaluations += sampled;
      }
      
      void CompleteMove(const Input& in) override
      {
        neighbors_accepted++;
      }
      
      /**
       At regular steps, the temperature is decreased
       multiplying it by a cooling rate.
       */
      void CompleteIteration(const Input& in) override
      {
        Runner<Input, State, CostStructure>::CompleteIteration(in);
        if (neighbors_sampled >= max_neighbors_sampled || neighbors_accepted >= max_neighbors_accepted)
        {
          temperature *= cooling_rate;
          neighbors_sampled = 0;
          neighbors_accepted = 0;
        }
      }
      
      // parameters
      void InitializeParameters() override
      {
        MoveRunner<StateManager, NeighborhoodExplorer>::InitializeParameters();
        compute_start_temperature("compute_start_temperature", "Should the runner compute the initial temperature?", this->parameters);
        start_temperature("start_temperature", "Starting temperature", this->parameters);
        cooling_rate("cooling_rate", "Cooling rate", this->parameters);
        max_neighbors_sampled("neighbors_sampled", "Maximum number of neighbors sampled at each temp.", this->parameters);
        max_neighbors_accepted("neighbors_accepted", "Maximum number of neighbor accepted at each temp.", this->parameters);
        if (!compute_start_temperature.IsSet())
          compute_start_temperature = false; // FIXME!!
      }
      
      Parameter<bool> compute_start_temperature;
      Parameter<double> start_temperature;
      Parameter<double> cooling_rate;
      Parameter<unsigned int> max_neighbors_sampled, max_neighbors_accepted;
      // state of SA
      double temperature; /**< The current temperature. */
      size_t neighbors_sampled, neighbors_accepted;
    };
  } // namespace Core
} // namespace EasyLocal
