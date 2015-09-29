#if !defined(_SIMULATED_ANNEALING_HH_)
#define _SIMULATED_ANNEALING_HH_

#include "easylocal/runners/abstractsimulatedannealing.hh"

namespace EasyLocal {
  
  namespace Core {
    
    /** Implements the Simulated annealing runner with a stop condition
     based on the minimum temperature.
     
     @ingroup Runners
     */
    template <class Input, class State, class Move, typename CFtype = int, class Compare = std::less<CostStructure<CFtype>>>
    class SimulatedAnnealing : public AbstractSimulatedAnnealing<Input, State, Move, CFtype, Compare>
    {
    public:
      
      SimulatedAnnealing(const Input& in,
                         StateManager<Input, State, CFtype>& sm,
                         NeighborhoodExplorer<Input, State, Move, CFtype>& ne,
                         std::string name);
      
      std::string StatusString() const;
      
    protected:
      void RegisterParameters();
      void InitializeRun() throw (ParameterNotSet, IncorrectParameterValue);
      bool StopCriterion();
      // parameters
      Parameter<double> min_temperature;
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
    template <class Input, class State, class Move, typename CFtype, class Compare>
    SimulatedAnnealing<Input, State, Move, CFtype, Compare>::SimulatedAnnealing(const Input& in,
                                                                       StateManager<Input, State, CFtype>& sm,
                                                                       NeighborhoodExplorer<Input, State, Move, CFtype>& ne,
                                                                       std::string name)
    : AbstractSimulatedAnnealing<Input, State, Move, CFtype, Compare>(in, sm, ne, name)
    {}
    
    template <class Input, class State, class Move, typename CFtype, class Compare>
    void SimulatedAnnealing<Input, State, Move, CFtype, Compare>::RegisterParameters()
    {
      AbstractSimulatedAnnealing<Input, State, Move, CFtype, Compare>::RegisterParameters();
      min_temperature("min_temperature", "Minimum temperature", this->parameters);
    }
    
    /**
     Initializes the run by invoking the companion superclass method, and
     setting the temperature to the start value.
     */
    template <class Input, class State, class Move, typename CFtype, class Compare>
    void SimulatedAnnealing<Input, State, Move, CFtype, Compare>::InitializeRun() throw (ParameterNotSet, IncorrectParameterValue)
    {
      if (min_temperature <= 0.0)
      {
        throw IncorrectParameterValue(min_temperature, "should be greater than zero");
      }
      AbstractSimulatedAnnealing<Input, State, Move, CFtype, Compare>::InitializeRun();
    }
    
    /**
     The search stops when a low temperature has reached.
     */
    template <class Input, class State, class Move, typename CFtype, class Compare>
    bool SimulatedAnnealing<Input, State, Move, CFtype, Compare>::StopCriterion()
    {
      return this->temperature <= min_temperature;
    }
    
    /**
     Create a string containing the status of the runner
     */
    template <class Input, class State, class Move, typename CFtype, class Compare>
    std::string SimulatedAnnealing<Input, State, Move, CFtype, Compare>::StatusString() const
    {
      std::stringstream status;
      status << "["
      << "Temp = " << this->temperature << " (" << this->start_temperature << "->" << this->min_temperature << "), "
      << "NS = " << this->neighbors_sampled << " (" << this->max_neighbors_sampled << "), "
      << "NA = " << this->neighbors_accepted  << " (" << this->max_neighbors_accepted << ")"
      << "]";
      return status.str();
    }
  }
}


#endif // _SIMULATED_ANNEALING_HH_
