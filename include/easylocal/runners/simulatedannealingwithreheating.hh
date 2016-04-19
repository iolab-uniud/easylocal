#if !defined(_SIMULATED_ANNEALING_WITH_REHEATING_HH_)
#define _SIMULATED_ANNEALING_WITH_REHEATING_HH_

#include "easylocal/runners/simulatedannealingevaluationbased.hh"

namespace EasyLocal {
  
  namespace Core {
    
    /** The Simulated annealing with Reheating runner relies on a probabilistic local
     search technique whose name comes from the fact that it
     simulates the cooling of a collection of hot vibrating atoms.
     
     At each iteration a candidate move is generated at random, and
     it is always accepted if it is an improving move.  Instead, if
     the move is a worsening one, the new solution is accepted with
     time decreasing probability.
     
     @ingroup Runners
     */
    template <class Input, class State, class Move, class CostStructure = DefaultCostStructure<int>>
    class SimulatedAnnealingWithReheating : public SimulatedAnnealingEvaluationBased<Input, State, Move, CostStructure>
    {
    public:
      
      SimulatedAnnealingWithReheating(const Input& in,
                                      StateManager<Input, State, CostStructure>& sm,
                                      NeighborhoodExplorer<Input, State, Move, CostStructure>& ne,
                                      std::string name);
      std::string StatusString() const;
    protected:
      bool StopCriterion();
      void CompleteMove();
      void InitializeRun() throw (ParameterNotSet, IncorrectParameterValue);
      bool ReheatCondition();
      // additional parameters
      void RegisterParameters();
      Parameter<double> first_reheat_ratio;
      Parameter<double> reheat_ratio;
      Parameter<double> first_descent_evaluations_share;
      Parameter<unsigned int> max_reheats;
      unsigned int reheats;
      unsigned int first_descent_evaluations, other_descents_evaluations;
    };
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    /**
     Constructs a simulated annealing runner by linking it to a state manager,
     a neighborhood explorer, and an input object.
     
     @param s a pointer to a compatible state manager
     @param ne a pointer to a compatible neighborhood explorer
     @param in a pointer to an input object
     */
    template <class Input, class State, class Move, class CostStructure>
    SimulatedAnnealingWithReheating<Input, State, Move, CostStructure>::SimulatedAnnealingWithReheating(const Input& in,
                                                                                                 StateManager<Input, State, CostStructure>& sm,
                                                                                                 NeighborhoodExplorer<Input, State, Move, CostStructure>& ne,
                                                                                                 std::string name)
    : SimulatedAnnealingEvaluationBased<Input, State, Move, CostStructure>(in, sm, ne, name)
    {}
    
    template <class Input, class State, class Move, class CostStructure>
    void SimulatedAnnealingWithReheating<Input, State, Move, CostStructure>::RegisterParameters()
    {
      AbstractSimulatedAnnealing<Input, State, Move, CostStructure>::RegisterParameters();
      first_reheat_ratio("first_reheat_ratio", "First reheat ratio", this->parameters);
      reheat_ratio("reheat_ratio", "Reheat ratio", this->parameters);
      first_descent_evaluations_share("first_descent_evaluations_share", "First descent cost function evaluations share", this->parameters);
      max_reheats("max_reheats", "Maximum number of reheats", this->parameters);
    }
    
    template <class Input, class State, class Move, class CostStructure>
    void SimulatedAnnealingWithReheating<Input, State, Move, CostStructure>::InitializeRun() throw (ParameterNotSet, IncorrectParameterValue)
    {
      SimulatedAnnealingEvaluationBased<Input, State, Move, CostStructure>::InitializeRun();
      reheats = 0;
      
      if (max_reheats > 0)
      {
        if (max_reheats > 1)
        {
          if (reheat_ratio <= 0.0)
            throw IncorrectParameterValue(reheat_ratio, "should be greater than zero");
            
            if (!first_reheat_ratio.IsSet())
              first_reheat_ratio = reheat_ratio;
        }
        
        if (first_reheat_ratio <= 0.0)
          throw IncorrectParameterValue(first_reheat_ratio, "should be greater than zero");
        if (first_descent_evaluations_share <= 0.0 || first_descent_evaluations_share > 1.0)
          throw IncorrectParameterValue(first_descent_evaluations_share, "should be a value in the interval ]0, 1]");
            
        this->max_neighbors_sampled = ceil(this->max_neighbors_sampled * first_descent_evaluations_share);
        first_descent_evaluations = this->max_evaluations * first_descent_evaluations_share;
        other_descents_evaluations = (this->max_evaluations - first_descent_evaluations) / max_reheats;
      }
      this->max_neighbors_accepted = ceil(this->max_neighbors_sampled * this->neighbors_accepted_ratio);      
    }
    
    /**
     A move is randomly picked.
     */
    template <class Input, class State, class Move, class CostStructure>
    void SimulatedAnnealingWithReheating<Input, State, Move, CostStructure>::CompleteMove()
    {
      SimulatedAnnealingEvaluationBased<Input, State, Move, CostStructure>::CompleteMove();
      if (ReheatCondition() && reheats <= max_reheats)
      {
        //     if (max_reheats != 0)
        //     {
        if (reheats == 0)
          this->start_temperature = this->start_temperature * first_reheat_ratio;
        else
          if (max_reheats > 1)
            this->start_temperature = this->start_temperature * reheat_ratio;
        //     }
        this->expected_number_of_temperatures = -log(this->start_temperature/this->expected_min_temperature) / log(this->cooling_rate);
        
        this->max_neighbors_sampled = other_descents_evaluations / this->expected_number_of_temperatures;
        this->max_neighbors_accepted = this->max_neighbors_sampled;
        reheats++;
        
        // std::cerr << reheats << " " << this->max_neighbors_sampled << " " << this->max_neighbors_accepted  << " " << this->start_temperature << " " << this->temperature << std::endl;
        this->temperature = this->start_temperature;
      }
    }
    
    template <class Input, class State, class Move, class CostStructure>
    bool SimulatedAnnealingWithReheating<Input, State, Move, CostStructure>::ReheatCondition()
    {
      if (max_reheats == 0)
        return false; //true;
      return this->evaluations >= first_descent_evaluations + other_descents_evaluations * reheats;
    }
    
    
    /**
     The search stops when a low temperature has reached.
     */
    template <class Input, class State, class Move, class CostStructure>
    bool SimulatedAnnealingWithReheating<Input, State, Move, CostStructure>::StopCriterion()
    {
      return reheats > max_reheats;
    }
    
    /**
     Create a string containing the status of the runner
     */
    template <class Input, class State, class Move, class CostStructure>
    std::string SimulatedAnnealingWithReheating<Input, State, Move, CostStructure>::StatusString() const
    {
      std::stringstream status;
      status << "["
      << "Temp = " << this->temperature << " (" << this->start_temperature << "), "
      << "NS = " << this->neighbors_sampled << " (" << this->max_neighbors_sampled << "), "
      << "NA = " << this->neighbors_accepted  << " (" << this->max_neighbors_accepted << "), "
      << "Reheats = " << reheats << " (" << max_reheats << ")"
      << "]";
      return status.str();
    }
  }
}

#endif // _SIMULATED_ANNEALING_WITH_REHEATING_HH_
