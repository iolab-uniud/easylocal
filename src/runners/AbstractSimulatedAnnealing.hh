#if !defined(_ABSTRACT_SIMULATED_ANNEALING_HH_)
#define _ABSTRACT_SIMULATED_ANNEALING_HH_

#include <runners/MoveRunner.hh>
#include <helpers/StateManager.hh>
#include <helpers/NeighborhoodExplorer.hh>
#include <cmath>
#include <stdexcept>
#include <algorithm>

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
template <class Input, class State, class Move, typename CFtype>
class AbstractSimulatedAnnealing : public MoveRunner<Input,State,Move,CFtype>
{
public:
  
  AbstractSimulatedAnnealing(const Input& in,
                     StateManager<Input,State,CFtype>& e_sm,
                     NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                     std::string name);
  
  void SetStartTemperature(double st)  { start_temperature = st; }
  void SetCoolingRate(double cr)  { cooling_rate = cr; }
  void SetMaxNeighborsSampled(unsigned int ns)  { max_neighbors_sampled = ns; }
  void SetMaxNeighborsAccepted(unsigned int na)  { max_neighbors_accepted = na; }
  void SetMaxIterations(unsigned long i)  { this->max_iterations = i; }
  
  unsigned int MaxNeighborsSampled() const { return max_neighbors_sampled; }
  unsigned int MaxNeighborsAccepted() const { return max_neighbors_accepted; }
  double StartTemperature() const { return start_temperature; }
  double CoolingRate() const { return cooling_rate; }
  double Temperature() const { return temperature; }
  unsigned long MaxIterations() const  { return this->max_iterations; }

protected:

  void InitializeRun() throw (ParameterNotSet, IncorrectParameterValue);
  void UpdateIterationCounter();
  void SelectMove();
  bool AcceptableMove();
  void CompleteMove();
  void CompleteIteration();
    // parameters
  Parameter<bool> compute_start_temperature;
  Parameter<double> start_temperature;
  Parameter<double> cooling_rate;
  Parameter<unsigned int> max_neighbors_sampled, max_neighbors_accepted;
    // state of SA
  double temperature; /**< The current temperature. */
  unsigned int neighbors_sampled, neighbors_accepted;
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
AbstractSimulatedAnnealing<Input,State,Move,CFtype>::AbstractSimulatedAnnealing(const Input& in,
                                                                StateManager<Input,State,CFtype>& e_sm,
                                                                NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                                                                std::string name)
: MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name, "Simulated Annealing Runner"),
  compute_start_temperature("compute_start_temperature", "Should the runner compute the initial temperature?", this->parameters),
  start_temperature("start_temperature", "Starting temperature", this->parameters),
  cooling_rate("cooling_rate", "Cooling rate", this->parameters),
  max_neighbors_sampled("neighbors_sampled", "Maximum number of neighbors sampled at each temp.", this->parameters),
  max_neighbors_accepted("neighbors_accepted", "Maximum number of neighbor accepted at each temp.", this->parameters)
{
  if (!compute_start_temperature.IsSet())
    compute_start_temperature = false; // FIXME!!
}


/**
 Initializes the run by invoking the companion superclass method, and
 setting the temperature to the start value.
 */
// FIXME
template <class Input, class State, class Move, typename CFtype>
void AbstractSimulatedAnnealing<Input,State,Move,CFtype>::InitializeRun() throw (ParameterNotSet, IncorrectParameterValue)
{
  MoveRunner<Input,State,Move,CFtype>::InitializeRun();

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
    std::vector<CFtype> cost_values(samples);
    //		double mean = 0.0, variance = 0.0;
    for (unsigned int i = 0; i < samples; i++)
    {
      //this->sm.RandomState(sampled_state);
      Move mv;
      this->ne.RandomMove(*this->p_current_state, mv);
      cost_values[i] = this->ne.DeltaCostFunction(*this->p_current_state, mv);
      //mean += cost_values[i];
    }
    /* mean /= samples;
     for (unsigned int i = 0; i < samples; i++)
     variance += (cost_values[i] - mean) * (cost_values[i] - mean) / samples;
     temperature = variance; */
    temperature = max(cost_values);
    /*From "An improved annealing scheme for the QAP. Connoly. EJOR 46 (1990) 93-100"
     temperature = min(cost_values.begin(), cost_values.end()) + (max(cost_values.begin(), cost_values.end()) - min(cost_values.begin(), cost_values.end()))/10;*/
  } 

  // If the number of maximum accepted neighbors for each temperature is not set, default to all of them 
  if (!max_neighbors_accepted.IsSet())
    max_neighbors_accepted = max_neighbors_sampled;
 
  neighbors_sampled = 0;
  neighbors_accepted = 0;
}

/**
 A move is randomly picked.
 */
template <class Input, class State, class Move, typename CFtype>
void AbstractSimulatedAnnealing<Input,State,Move,CFtype>::SelectMove()
{
  this->ne.RandomMove(*this->p_current_state, this->current_move);
  this->current_move_cost = this->ne.DeltaCostFunction(*this->p_current_state, this->current_move);
  if (this->observer != nullptr)
    {
      this->current_move_violations = this->ne.DeltaViolations(*this->p_current_state, this->current_move);
    }
  neighbors_sampled++;
}

/**
 A move is randomly picked.
 */
template <class Input, class State, class Move, typename CFtype>
void AbstractSimulatedAnnealing<Input,State,Move,CFtype>::CompleteMove()
{
  neighbors_accepted++;
}

/**  
At regular steps, the temperature is decreased 
   multiplying it by a cooling rate.
*/
template <class Input, class State, class Move, typename CFtype>
void AbstractSimulatedAnnealing<Input,State,Move,CFtype>::CompleteIteration()
{
  if (neighbors_sampled == max_neighbors_sampled || neighbors_accepted == max_neighbors_accepted)
    {      
      temperature *= cooling_rate;
      neighbors_sampled = 0;
      neighbors_accepted = 0;
    }
}

/** A move is surely accepted if it improves the cost function
 or with exponentially decreasing probability if it is 
 a worsening one.
 */
template <class Input, class State, class Move, typename CFtype>
bool AbstractSimulatedAnnealing<Input,State,Move,CFtype>::AcceptableMove()
{ 
  return LessOrEqualThan(this->current_move_cost,(CFtype)0)
    || (Random::Double() < exp(-this->current_move_cost/temperature)); 
}

#endif // _ABSTRACT_SIMULATED_ANNEALING_HH_
