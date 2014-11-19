#if !defined(_SIMULATED_ANNEALING_ITERATION_BASED_HH_)
#define _SIMULATED_ANNEALING_ITERATION_BASED_HH_

#include "easylocal/runners/abstractsimulatedannealing.hh"

namespace EasyLocal {
  
  namespace Core {
    
    /** Implements the Simulated annealing runner with a stop condition
     based on the number of iterations. In addition, the number of neighbors
     sampled at each iteration is computed in such a way that the total number
     of iterations is fixed
     
     @ingroup Runners
     */
    
    template <class Input, class State, class Move, typename CFtype = int>
    class SimulatedAnnealingEvaluationBased : public AbstractSimulatedAnnealing<Input, State, Move, CFtype>
    {
    public:
      
      SimulatedAnnealingEvaluationBased(const Input& in,
                                       StateManager<Input, State, CFtype>& e_sm,
                                       NeighborhoodExplorer<Input, State, Move, CFtype>& e_ne,
                                       std::string name);
      std::string StatusString();
      
      void SetTemperatureRange(double tr) { temperature_range = tr; }
      double TemperatureRange() const { return temperature_range; }
      void SetNeighborsAcceptedRatio(double nar) { neighbors_accepted_ratio = nar; }
      double NeighborsAcceptedRatio() const { return neighbors_accepted_ratio; }
      
    protected:
      void RegisterParameters();
      void InitializeRun() throw (ParameterNotSet, IncorrectParameterValue);
      bool StopCriterion();
      void CompleteIteration();
      
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
     @param in a poiter to an input object
     */
    template <class Input, class State, class Move, typename CFtype>
    SimulatedAnnealingEvaluationBased<Input, State, Move, CFtype>::SimulatedAnnealingEvaluationBased(const Input& in,
                                                                                                   StateManager<Input, State, CFtype>& e_sm,
                                                                                                   NeighborhoodExplorer<Input, State, Move, CFtype>& e_ne,
                                                                                                   std::string name)
    : AbstractSimulatedAnnealing<Input, State, Move, CFtype>(in, e_sm, e_ne, name)
    {}
    
    template <class Input, class State, class Move, typename CFtype>
    void SimulatedAnnealingEvaluationBased<Input, State, Move, CFtype>::RegisterParameters()
    {
      AbstractSimulatedAnnealing<Input, State, Move, CFtype>::RegisterParameters();
      neighbors_accepted_ratio("neighbors_accepted_ratio", "Ratio of neighbors accepted", this->parameters);
      temperature_range("temperature_range", "Temperature_range", this->parameters);
    }        
    
    /**
     Initializes the run by invoking the companion superclass method, and
     setting the temperature to the start value.
     */
    template <class Input, class State, class Move, typename CFtype>
    void SimulatedAnnealingEvaluationBased<Input, State, Move, CFtype>::InitializeRun() throw (ParameterNotSet, IncorrectParameterValue)
    {      
      AbstractSimulatedAnnealing<Input, State, Move, CFtype>::InitializeRun();
      expected_min_temperature = this->start_temperature / temperature_range;
      expected_number_of_temperatures = - log(temperature_range) / log(this->cooling_rate);
      
      this->max_neighbors_sampled = static_cast<unsigned>(this->max_evaluations / expected_number_of_temperatures);
      
      // If the ratio of accepted neighbors for each temperature is not set,
      if (!neighbors_accepted_ratio.IsSet())
        this->max_neighbors_accepted = this->max_neighbors_sampled;
        else
          this->max_neighbors_accepted = static_cast<unsigned>(this->max_neighbors_sampled * neighbors_accepted_ratio);
    }
    
    
    /**
     The search stops when the number of iterations is expired (already checked in the superclass MoveRunner)
     */
    template <class Input, class State, class Move, typename CFtype>
    bool SimulatedAnnealingEvaluationBased<Input, State, Move, CFtype>::StopCriterion()
    {
      return false;
    }
    
    template <class Input, class State, class Move, typename CFtype>
    void SimulatedAnnealingEvaluationBased<Input, State, Move, CFtype>::CompleteIteration()
    {
      AbstractSimulatedAnnealing<Input, State, Move, CFtype>::CompleteIteration();
      // the iteration counter was already incremented by PrepareIteration, so to make it consistent with the overall number of sampled neighbors
      // the value should be decreased by 1
      this->iteration += this->neighbors_sampled - 1;
    }
    
    /**
     Create a string containing the status of the runner
     */
    template <class Input, class State, class Move, typename CFtype>
    std::string SimulatedAnnealingEvaluationBased<Input, State, Move, CFtype>::StatusString()
    {
      std::stringstream status;
      status << "["
      << "Temp = " << this->temperature << " (" << this->start_temperature << "), "
      << "NS = " << this->neighbors_sampled << " (" << this->max_neighbors_sampled << "), "
      << "NA = " << this->neighbors_accepted  << " (" << this->max_neighbors_accepted << ")"
      << "]";
      return status.str();
    }
  }
}

#endif // _SIMULATED_ANNEALING_HH_
