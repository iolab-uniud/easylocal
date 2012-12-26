#if !defined(_SIMULATED_ANNEALING_WITH_REHEATING_HH_)
#define _SIMULATED_ANNEALING_WITH_REHEATING_HH_

#include <runners/SimulatedAnnealingIterationBased.hh>

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
class SimulatedAnnealingWithReheating : public SimulatedAnnealingIterationBased<Input,State,Move,CFtype>
{
public:
  
  SimulatedAnnealingWithReheating(const Input& in,
                     StateManager<Input,State,CFtype>& e_sm,
                     NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                     std::string name);
  
  void SetReheat(double rst)  { reheat_ratio = rst; }
  void SetFirstReheat(double rst)  { first_reheat_ratio = rst; } // applied only to the first round
  void SetFirstDescentIterationsShare(double r) { first_descent_iterations_share = r; } // the percentage of max_iterations granted to the first descent
  std::string StatusString();
protected:
  bool StopCriterion();
  void CompleteMove();
  void InitializeRun() throw (ParameterNotSet, IncorrectParameterValue);
  bool ReheatCondition();
  // additional parameters
  Parameter<double> first_reheat_ratio;
  Parameter<double> reheat_ratio;
  Parameter<double> first_descent_iterations_share;
  Parameter<unsigned int> max_reheats;
  unsigned int reheats;
  unsigned int first_descent_iterations, other_descent_iterations;
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
: SimulatedAnnealingIterationBased<Input,State,Move,CFtype>(in, e_sm, e_ne, name),
  first_reheat_ratio("first_reheat_ratio", "First reheat ratio", this->parameters),
  reheat_ratio("reheat_ratio", "Reheat ratio", this->parameters),
  first_descent_iterations_share("first_descent_iterations_share", "First descent iterations share", this->parameters),
  max_reheats("max_reheats", "Maximum number of reheats", this->parameters)
{
}

template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealingWithReheating<Input,State,Move,CFtype>::InitializeRun() throw (ParameterNotSet, IncorrectParameterValue)
{
  SimulatedAnnealingIterationBased<Input,State,Move,CFtype>::InitializeRun();
  
  if (reheat_ratio <= 0.0)
    throw IncorrectParameterValue(reheat_ratio, "should be greater than zero");
  if (!first_reheat_ratio.IsSet())
    first_reheat_ratio = reheat_ratio;
  if (first_reheat_ratio <= 0.0)
    throw IncorrectParameterValue(first_reheat_ratio, "should be greater than zero");
  if (first_descent_iterations_share <= 0.0 || first_descent_iterations_share > 1.0)
    throw IncorrectParameterValue(first_descent_iterations_share, "should be a value in the interval ]0, 1]");  
  
  reheats = 0;

  this->max_neighbors_sampled = ceil(this->max_neighbors_sampled * first_descent_iterations_share);
  this->max_neighbors_accepted = ceil(this->max_neighbors_sampled * this->neighbors_accepted_ratio);

  first_descent_iterations = this->max_iterations * first_descent_iterations_share;
  other_descent_iterations = (this->max_iterations - first_descent_iterations)/max_reheats;     
}

/**
 A move is randomly picked.
 */
template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealingWithReheating<Input,State,Move,CFtype>::CompleteMove()
{
  SimulatedAnnealingIterationBased<Input,State,Move,CFtype>::CompleteMove();
  if (ReheatCondition()
       && reheats <= max_reheats)
  {
    if (reheats == 0)
      this->start_temperature = this->start_temperature * first_reheat_ratio;
    else
      this->start_temperature = this->start_temperature * reheat_ratio;
        
    this->expected_number_of_temperatures = -log(this->start_temperature/this->expected_min_temperature) / log(this->cooling_rate);

    this->max_neighbors_sampled = other_descent_iterations / this->expected_number_of_temperatures;
    this->max_neighbors_accepted = this->max_neighbors_sampled;
    reheats++;

    std::cerr << reheats << " " << this->max_neighbors_sampled << " " << this->max_neighbors_accepted 
	      << " " << this->start_temperature << " " << this->temperature << std::endl;
    this->temperature = this->start_temperature;    
  }
}

template <class Input, class State, class Move, typename CFtype>
bool SimulatedAnnealingWithReheating<Input,State,Move,CFtype>::ReheatCondition()
{
  unsigned int stop_iteration;
  stop_iteration = first_descent_iterations + other_descent_iterations * reheats;
  return this->iteration >= stop_iteration;
}


/**
 The search stops when a low temperature has reached.
 */
template <class Input, class State, class Move, typename CFtype>
bool SimulatedAnnealingWithReheating<Input,State,Move,CFtype>::StopCriterion()
{
  return reheats > max_reheats;
}

/**
 Create a string containing the status of the runner
 */
template <class Input, class State, class Move, typename CFtype>
std::string SimulatedAnnealingWithReheating<Input,State,Move,CFtype>::StatusString()
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
#endif
