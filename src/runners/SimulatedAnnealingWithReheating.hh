#if !defined(_SIMULATED_ANNEALING_WITH_REHEATING_HH_)
#define _SIMULATED_ANNEALING_WITH_REHEATING_HH_

#include <runners/SimulatedAnnealing.hh>

/** The Simulated annealing with Reheating runner relies on a probabilistic local
 search technique whose name comes from the fact that it
 simulates the cooling of a collection of hot vibrating atoms.
 
 At each iteration a candidate move is generated at random, and
 it is always accepted if it is an improving move.  Instead, if
 the move is a worsening one, the new solution is accepted with
 time decreasing probability.
 
 @ingroup Runners
 */
template <class Input, class State, class Move, typename CFtype>
class SimulatedAnnealingWithReheating : public SimulatedAnnealing<Input,State,Move,CFtype>
{
public:
  
  SimulatedAnnealingWithReheating(const Input& in,
                     StateManager<Input,State,CFtype>& e_sm,
                     NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                     std::string name);
  
  void SetReheat(double rst)  { reheat = rst; }
  void SetFirstReheat(double rst)  { first_reheat = rst; } // applied only to the first round
  void SetFirstDescentIterationsRatio(double r) { first_descent_iterations_ratio = r; } // the percentage of max_iterations granted to the first descent
protected:
  bool StopCriterion();
  void CompleteMove();
  void InitializeRun() throw (ParameterNotSet, IncorrectParameterValue);
  // parameters
  Parameter<double> first_reheat;
  Parameter<double> reheat;
  Parameter<double> first_descent_iterations_ratio;
  Parameter<unsigned int> max_reheats;
  unsigned int reheats;
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
SimulatedAnnealingWithReheating<Input,State,Move,CFtype>::SimulatedAnnealingWithReheating(const Input& in,
                                                                StateManager<Input,State,CFtype>& e_sm,
                                                                NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                                                                std::string name)
: SimulatedAnnealing<Input,State,Move,CFtype>(in, e_sm, e_ne, name),
  first_reheat("first_reheat", "First reheat ratio", this->parameters),
  reheat("reheat", "Reheat ratio", this->parameters),
  first_descent_iterations_ratio("first_descent_iterations_ratio", "First descent iterations ratio", this->parameters),
  max_reheats("max_reheats", "Maximum number of reheats", this->parameters)
{
}

template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealingWithReheating<Input,State,Move,CFtype>::InitializeRun() throw (ParameterNotSet, IncorrectParameterValue)
{
  SimulatedAnnealing<Input,State,Move,CFtype>::InitializeRun();
  
  if (reheat <= 0.0)
    throw IncorrectParameterValue(reheat, "should be greater than zero");
  if (!first_reheat.IsSet())
    first_reheat = reheat;
  if (first_reheat <= 0.0)
    throw IncorrectParameterValue(first_reheat, "should be greater than zero");
  if (first_descent_iterations_ratio <= 0.0 || first_descent_iterations_ratio > 1.0)
    throw IncorrectParameterValue(first_descent_iterations_ratio, "should be a value in the interval ]0, 1]");  
  
  reheats = 0;
//   unsigned int number_of_temperatures = -log(this->start_temperature/this->min_temperature) / log(this->cooling_rate);
//   this->max_neighbors_sampled = ceil((first_descent_iterations_ratio * this->max_iterations) /number_of_temperatures);
  //  this->max_neighbors_accepted = this->max_neighbors_sampled;
}

/**
 A move is randomly picked.
 */
template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealingWithReheating<Input,State,Move,CFtype>::CompleteMove()
{
  SimulatedAnnealing<Input,State,Move,CFtype>::CompleteMove();
  if (SimulatedAnnealing<Input,State,Move,CFtype>::StopCriterion() && reheats <= max_reheats)
  {
    if (reheats == 0)
      {
	this->start_temperature = this->start_temperature * first_reheat;
      }
    else
      this->start_temperature = this->start_temperature * reheat;
    
    this->temperature = this->start_temperature;
    
    std::cerr << "Reheat " << this->temperature << std::endl;
    
    unsigned int number_of_temperatures = -log(this->start_temperature / this->min_temperature) / log(this->cooling_rate);
    this->max_neighbors_sampled = ceil(((1.0 - first_descent_iterations_ratio) * this->max_iterations) / (max_reheats * number_of_temperatures));
    this->max_neighbors_accepted = this->max_neighbors_sampled;
    reheats++;
  }
}

/**
 The search stops when a low temperature has reached.
 */
template <class Input, class State, class Move, typename CFtype>
bool SimulatedAnnealingWithReheating<Input,State,Move,CFtype>::StopCriterion()
{
  return SimulatedAnnealing<Input,State,Move,CFtype>::StopCriterion() && reheats == max_reheats;
}

#endif
