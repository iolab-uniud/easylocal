#if !defined(_SIMULATED_ANNEALING_HH_)
#define _SIMULATED_ANNEALING_HH_

#include "easylocal/runners/abstractsimulatedannealing.hh"

namespace EasyLocal {
  
  namespace Core {
    
    /** Implements the Simulated annealing runner with a stop condition
     based on the minimum temperature.
     
     @ingroup Runners
     */
    template <class Input, class State, class Move, typename CFtype = int>
    class SimulatedAnnealing : public AbstractSimulatedAnnealing<Input, State, Move, CFtype>
    {
    public:
      
      SimulatedAnnealing(const Input& in,
                         StateManager<Input, State, CFtype>& e_sm,
                         NeighborhoodExplorer<Input, State, Move, CFtype>& e_ne,
                         std::string name);
      
      void SetMinTemperature(double st)  { min_temperature = st; }
      double MinTemperature() const { return min_temperature; }
      std::string StatusString();
      
    protected:
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
    template <class Input, class State, class Move, typename CFtype>
    SimulatedAnnealing<Input, State, Move, CFtype>::SimulatedAnnealing(const Input& in,
                                                                       StateManager<Input, State, CFtype>& e_sm,
                                                                       NeighborhoodExplorer<Input, State, Move, CFtype>& e_ne,
                                                                       std::string name)
    : AbstractSimulatedAnnealing<Input, State, Move, CFtype>(in, e_sm, e_ne, name),
    min_temperature("min_temperature", "Minimum temperature", this->parameters)
    {}
    
    /**
     Initializes the run by invoking the companion superclass method, and
     setting the temperature to the start value.
     */
    // FIXME
    template <class Input, class State, class Move, typename CFtype>
    void SimulatedAnnealing<Input, State, Move, CFtype>::InitializeRun() throw (ParameterNotSet, IncorrectParameterValue)
    {
      if (min_temperature <= 0.0)
        throw IncorrectParameterValue(min_temperature, "should be greater than zero");
        AbstractSimulatedAnnealing<Input, State, Move, CFtype>::InitializeRun();
        }
    
    /**
     The search stops when a low temperature has reached.
     */
    template <class Input, class State, class Move, typename CFtype>
    bool SimulatedAnnealing<Input, State, Move, CFtype>::StopCriterion()
    {
      return this->temperature <= min_temperature;
    }
    
    /**
     Create a string containing the status of the runner
     */
    template <class Input, class State, class Move, typename CFtype>
    std::string SimulatedAnnealing<Input, State, Move, CFtype>::StatusString()
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
