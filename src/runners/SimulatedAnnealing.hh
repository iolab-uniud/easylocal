#if !defined(_SIMULATED_ANNEALING_HH_)
#define _SIMULATED_ANNEALING_HH_

#include <runners/MoveRunner.hh>
#include <helpers/StateManager.hh>
#include <helpers/NeighborhoodExplorer.hh>
#include <cmath>
#include <stdexcept>

/** The Simulated annealing runner relies on a probabilistic local
 search technique whose name comes from the fact that it
 simulates the cooling of a collection of hot vibrating atoms.
 
 At each iteration a candidate move is generated at random, and
 it is always accepted if it is an improving move.  Instead, if
 the move is a worsening one, the new solution is accepted with
 time decreasing probability.
 
 @ingroup Runners
 */
template <class Input, class State, class Move, typename CFtype = int>
class SimulatedAnnealing : public MoveRunner<Input,State,Move,CFtype>
{
public:
  
  SimulatedAnnealing(const Input& in,
                     StateManager<Input,State,CFtype>& e_sm,
                     NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                     std::string name,
                     CLParser& cl = CLParser::empty);	
  
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
  void Print(std::ostream& os = std::cout) const;
  void SetStartTemperature(double st)  { start_temperature = st; }
  void SetRestartTemperatureRatio(double rst)  { restart_temperature_ratio = rst; }
  void SetRestartTemperatureRatioFirstRound(double rst)  { restart_temperature_ratio_first_round = rst; } // applied only to the first round
  void SetMinTemperature(double st)  { min_temperature = st; }
  void SetCoolingRate(double cr)  { cooling_rate = cr; }
  void SetMaxNeighborsSampled(unsigned int ns)  { max_neighbors_sampled = ns; }
  void SetMaxNeighborsAccepted(unsigned int na)  { max_neighbors_accepted = na; }
  void SetIterationsRatio(double ir) { iterations_ratio = ir; } // the percentage of max_iterations granted to all the rounds after the first (the first round has max_iterations)

  unsigned int MaxNeighborsSampled() const { return max_neighbors_sampled; }
  unsigned int MaxNeighborsAccepted() const { return max_neighbors_accepted; }
  double StartTemperature() const { return start_temperature; }
  double MinTemperature() const { return min_temperature; }
  double CoolingRate() const { return cooling_rate; }

  double Temperature() const { return temperature; }

protected:
  void GoCheck() const;
  void InitializeRun(unsigned rounds = 0, unsigned max_rounds = 1);
  bool StopCriterion();
  void UpdateIterationCounter();
  void SelectMove();
  bool AcceptableMove();
  void StoreMove();
  // parameters
  double temperature; /**< The current temperature. */
  double start_temperature;
  double min_temperature;
  double cooling_rate;
  double restart_temperature_ratio;
  double restart_temperature_ratio_first_round;
  double iterations_ratio;
  unsigned int max_neighbors_sampled, neighbors_sampled, max_neighbors_accepted, neighbors_accepted;
  ArgumentGroup simulated_annealing_arguments;
  ValArgument<double> arg_start_temperature, arg_min_temperature, arg_cooling_rate, arg_restart_temperature_ratio;
  ValArgument<unsigned int> arg_neighbors_sampled, arg_neighbors_accepted;
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
SimulatedAnnealing<Input,State,Move,CFtype>::SimulatedAnnealing(const Input& in,
                                                                StateManager<Input,State,CFtype>& e_sm,
                                                                NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                                                                std::string name,
                                                                CLParser& cl)
: MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name),
start_temperature(0.0), min_temperature(0.0001), cooling_rate(0.75), restart_temperature_ratio(1.0), restart_temperature_ratio_first_round(1.0), iterations_ratio(1.0), max_neighbors_sampled(10), max_neighbors_accepted(neighbors_sampled),
simulated_annealing_arguments("sa_" + name, "sa_" + name, false), arg_start_temperature("start_temperature", "st", false),
arg_min_temperature("min_temperature", "mt", false), arg_cooling_rate("cooling_rate", "cr", true), 
arg_restart_temperature_ratio("restart_temperature_ratio", "rst", false), 
arg_neighbors_sampled("neighbors_sampled", "ns", true),
arg_neighbors_accepted("neighbors_accepted", "na", false)
{
  simulated_annealing_arguments.AddArgument(arg_start_temperature);
  simulated_annealing_arguments.AddArgument(arg_min_temperature);
  simulated_annealing_arguments.AddArgument(arg_cooling_rate);
  simulated_annealing_arguments.AddArgument(arg_restart_temperature_ratio);
  simulated_annealing_arguments.AddArgument(arg_neighbors_sampled);
  simulated_annealing_arguments.AddArgument(arg_neighbors_accepted);
  
  cl.AddArgument(simulated_annealing_arguments);
  cl.MatchArgument(simulated_annealing_arguments);
  if (simulated_annealing_arguments.IsSet())
  {
    if (arg_start_temperature.IsSet())
      start_temperature = arg_start_temperature.GetValue();
    if (arg_min_temperature.IsSet())
      min_temperature = arg_min_temperature.GetValue();
    cooling_rate = arg_cooling_rate.GetValue();
    if (arg_restart_temperature_ratio.IsSet())
      restart_temperature_ratio = arg_restart_temperature_ratio.GetValue();
    max_neighbors_sampled = arg_neighbors_sampled.GetValue();
    if (arg_neighbors_accepted.IsSet())
      max_neighbors_accepted = arg_neighbors_accepted.GetValue();
  }
}

template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealing<Input,State,Move,CFtype>::Print(std::ostream& os) const
{
  os  << "Simulated Annealing Runner: " << std::endl;
  os  << "  Max iterations: " << this->max_iteration << std::endl;
  os  << "  Start temperature: " << start_temperature << std::endl;
  os  << "  Min temperature: " << min_temperature << std::endl;
  os  << "  Cooling rate: " << cooling_rate << std::endl;
  os  << "  Neighbors sampled: " << max_neighbors_sampled << std::endl;
  os  << "  Neighbors accepted: " << max_neighbors_accepted << std::endl;
  os  << "  Restart temperature ratio: " << restart_temperature_ratio << std::endl;
  os  << "  Restart temperature ratio first round: " << restart_temperature_ratio_first_round << std::endl;
  os  << "  Iterations ratio: " << iterations_ratio << std::endl;
}

template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealing<Input,State,Move,CFtype>::GoCheck() const

{
  if (start_temperature < 0)
    throw std::logic_error("negative start_temparature for object " + this->name);
  if (cooling_rate <= 0)
    throw std::logic_error("negative cooling_rate for object " + this->name);
  if (max_neighbors_sampled == 0)
    throw std::logic_error("Max_neighbors_sampled is zero for object " + this->name);
  if (max_neighbors_accepted == 0)
    throw std::logic_error("max_neighbors_accepted is zero for object " + this->name);
  if (restart_temperature_ratio < 0)
    throw std::logic_error("negative restart_temparature_ratio for object " + this->name);
 if (restart_temperature_ratio_first_round < 0)
    throw std::logic_error("negative restart_temparature_ratio_first_round for object " + this->name);
 if (iterations_ratio < 0)
    throw std::logic_error("negative iterations_ratio for object " + this->name);

}

template <typename CFtype>
CFtype max(const std::vector<CFtype>& values) 
{
  CFtype max_val = values[0];
  for (unsigned int i = 1; i < values.size(); i++)
    if (values[i] > max_val)
      max_val = values[i];
  
  return max_val;
}

template <typename CFtype>
CFtype min(const std::vector<CFtype>& values) 
{
  CFtype min_val = values[0];
  for (unsigned int i = 1; i < values.size(); i++)
    if (values[i] < min_val)
      min_val = values[i];
  
  return min_val;
}

/**
 Initializes the run by invoking the companion superclass method, and
 setting the temperature to the start value.
 */
template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealing<Input,State,Move,CFtype>::InitializeRun(unsigned rounds, unsigned max_rounds)
{
  
  MoveRunner<Input,State,Move,CFtype>::InitializeRun();

  if(rounds == 0)
    {
      if (start_temperature > 0.0)
	temperature = start_temperature;
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
	      this->ne.RandomMove(this->current_state, mv);
	      cost_values[i] = this->ne.DeltaCostFunction(this->current_state, mv);
	      //mean += cost_values[i];
	    }
	  /* mean /= samples;
	     for (unsigned int i = 0; i < samples; i++)
	     variance += (cost_values[i] - mean) * (cost_values[i] - mean) / samples;
	     temperature = variance; */
	  temperature = max(cost_values);
	  /*From "An improved annealing scheme for the QAP. Connoly. EJOR 46 (1990) 93-100"
	    temperature = min(cost_values) + (max(cost_values) - min(cost_values))/10;*/
	}

    }
  else
    { 
      if(rounds == 1) 
	{
	  start_temperature = start_temperature*restart_temperature_ratio_first_round;
	  this->max_iteration = this->max_iteration*iterations_ratio/(max_rounds-1);
	}
      else
	start_temperature = start_temperature*restart_temperature_ratio;

      temperature = start_temperature;
      unsigned  number_of_temperatures = -log(temperature/min_temperature)/log(cooling_rate);
      max_neighbors_sampled = ceil(this->max_iteration/number_of_temperatures);
      max_neighbors_accepted = max_neighbors_sampled; 
    }

  neighbors_sampled = 0;
  neighbors_accepted = 0;
}

/**
 A move is randomly picked.
 */
template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealing<Input,State,Move,CFtype>::SelectMove()
{
  this->ne.RandomMove(this->current_state, this->current_move);
  this->ComputeMoveCost();
  neighbors_sampled++;
}

/**
 A move is randomly picked.
 */
template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealing<Input,State,Move,CFtype>::StoreMove()
{
  if (this->observer != NULL)
    this->observer->NotifyStoreMove(*this);
  if (LessOrEqualThan(this->current_state_cost, this->best_state_cost))
    {
      this->best_state = this->current_state; // Change best_state in case of equal cost to improve diversification
      if (LessThan(this->current_state_cost, this->best_state_cost))
	{
	  if (this->observer != NULL)
	    this->observer->NotifyNewBest(*this);      
	  this->best_state_cost = this->current_state_cost;
	  this->iteration_of_best = this->number_of_iterations;
	}
    }
  neighbors_accepted++;
}

template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealing<Input,State,Move,CFtype>::ReadParameters(std::istream& is, std::ostream& os)

{
  os << "SIMULATED ANNEALING -- INPUT PARAMETERS" << std::endl;
  os << "  Start temperature: ";
  is >> start_temperature;
  os << "  Min temperature: ";
  is >> min_temperature;
  os << "  Cooling rate: ";
  is >> cooling_rate;
  os << "  Neighbors sampled at each temperature: ";
  is >> max_neighbors_sampled;
  os << "  Neighbors accepted at each temperature: ";
  is >> max_neighbors_accepted;
  os << "  Restart temperature ratio: ";
  is >> restart_temperature_ratio;
  os << "  Restart temperature ratio first round: ";
  is >> restart_temperature_ratio_first_round;
  os << "  Iterations ratio: ";
  is >> iterations_ratio;
}

/**
 The search stops when a low temperature has reached.
 */
template <class Input, class State, class Move, typename CFtype>
bool SimulatedAnnealing<Input,State,Move,CFtype>::StopCriterion()
{ 
  return temperature <= min_temperature; // && this->max_iteration == ULONG_MAX;
}

/**
 At regular steps, the temperature is decreased 
 multiplying it by a cooling rate.
 */
template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealing<Input,State,Move,CFtype>::UpdateIterationCounter()
{
  MoveRunner<Input,State,Move,CFtype>::UpdateIterationCounter();
  if (neighbors_sampled == max_neighbors_sampled || neighbors_accepted == max_neighbors_accepted)
    {
//       std::cerr << neighbors_accepted << "/" << neighbors_sampled  << std::endl;
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
bool SimulatedAnnealing<Input,State,Move,CFtype>::AcceptableMove()
{ 
  return LessOrEqualThan(this->current_move_cost,(CFtype)0)
  || (Random::Double_Unit_Uniform() < exp(-this->current_move_cost/temperature)); 
}

#endif // _SIMULATED_ANNEALING_HH_
