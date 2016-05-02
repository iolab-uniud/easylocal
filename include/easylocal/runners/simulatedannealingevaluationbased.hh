#if !defined(_SIMULATED_ANNEALING_ITERATION_BASED_HH_)
#define _SIMULATED_ANNEALING_ITERATION_BASED_HH_

#include "easylocal/runners/abstractsimulatedannealing.hh"

namespace EasyLocal {
  
  namespace Core {
    
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
      
      SimulatedAnnealingEvaluationBased(const Input& in,
                                       StateManager<Input, State, CostStructure>& e_sm,
                                       NeighborhoodExplorer<Input, State, Move, CostStructure>& e_ne,
                                       std::string name);
      std::string StatusString() const;
    protected:
      void InitializeParameters();
      void InitializeRun() throw (ParameterNotSet, IncorrectParameterValue);
      bool StopCriterion();
      
      // additional parameters
      Parameter<double>  neighbors_accepted_ratio;
      Parameter<double> temperature_range;
      unsigned int expected_number_of_temperatures;
      double expected_min_temperature;
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
    SimulatedAnnealingEvaluationBased<Input, State, Move, CostStructure>::SimulatedAnnealingEvaluationBased(const Input& in,
                                                                                                   StateManager<Input, State, CostStructure>& e_sm,
                                                                                                   NeighborhoodExplorer<Input, State, Move, CostStructure>& e_ne,
                                                                                                   std::string name)
    : AbstractSimulatedAnnealing<Input, State, Move, CostStructure>(in, e_sm, e_ne, name)
    {}
    
    template <class Input, class State, class Move, class CostStructure>
    void SimulatedAnnealingEvaluationBased<Input, State, Move, CostStructure>::InitializeParameters()
    {
      AbstractSimulatedAnnealing<Input, State, Move, CostStructure>::InitializeParameters();
      neighbors_accepted_ratio("neighbors_accepted_ratio", "Ratio of neighbors accepted", this->parameters);
      temperature_range("temperature_range", "Temperature_range", this->parameters);
      this->max_neighbors_sampled = this->max_neighbors_accepted = 0;
    }        
    
    /**
     Initializes the run by invoking the companion superclass method, and
     setting the temperature to the start value.
     */
    template <class Input, class State, class Move, class CostStructure>
    void SimulatedAnnealingEvaluationBased<Input, State, Move, CostStructure>::InitializeRun() throw (ParameterNotSet, IncorrectParameterValue)
    {      
      AbstractSimulatedAnnealing<Input, State, Move, CostStructure>::InitializeRun();
      expected_min_temperature = this->start_temperature / temperature_range;
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
    bool SimulatedAnnealingEvaluationBased<Input, State, Move, CostStructure>::StopCriterion()
    {
      return false;
    }
    
    /**
     Create a string containing the status of the runner
     */
    template <class Input, class State, class Move, class CostStructure>
    std::string SimulatedAnnealingEvaluationBased<Input, State, Move, CostStructure>::StatusString() const
    {
      std::stringstream status;
      status << "["
      << "Temp = " << this->temperature << " (" << this->start_temperature << "), "
      << "NS = " << this->neighbors_sampled << " (" << this->max_neighbors_sampled << "), "
      << "NA = " << this->neighbors_accepted  << " (" << this->max_neighbors_accepted << "), "
      << "Eval = " << this->evaluations
      << "]";
      return status.str();
    }
  }
}

#endif // _SIMULATED_ANNEALING_HH_
