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
template <class Input, class State, class Move, typename CFtype = int>
class SimulatedAnnealingWithReheating : public SimulatedAnnealing<Input,State,Move,CFtype>
{
public:
  
  SimulatedAnnealingWithReheating(const Input& in,
                     StateManager<Input,State,CFtype>& e_sm,
                     NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                     std::string name,
                     CLParser& cl = CLParser::empty);
  
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
  void Print(std::ostream& os = std::cout) const;
  void SetReheat(double rst)  { reheat = rst; }
  void SetFirstReheat(double rst)  { first_reheat = rst; } // applied only to the first round
  void SetFirstDescentIterationsRatio(double r) { first_descent_iterations_ratio = r; } // the percentage of max_iterations granted to the first descent
protected:
  bool StopCriterion();
  void CompleteMove();
  void InitializeRun();
  // parameters
  double first_reheat;
  double reheat;
  double first_descent_iterations_ratio;
  unsigned int reheats, max_reheats;
  ArgumentGroup simulated_annealing_with_reheating_arguments;
  ValArgument<double> arg_first_reheat, arg_reheat, arg_first_descent_iterations_ratio;
  ValArgument<unsigned int> arg_max_reheats;
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
                                                                std::string name,
                                                                CLParser& cl)
: SimulatedAnnealing<Input,State,Move,CFtype>(in, e_sm, e_ne, name), first_reheat(1.0), reheat(1.0), first_descent_iterations_ratio(0.5), max_reheats(10),simulated_annealing_with_reheating_arguments("sawr_" + name, "sawr_" + name, false), arg_first_reheat("first_reheat", "frh", false), arg_reheat("reheat", "rh", true),  arg_first_descent_iterations_ratio("first_descent_iterations_ratio", "fdir", false),arg_max_reheats("max_reheats", "mr", true)
{
  simulated_annealing_with_reheating_arguments.AddArgument(this->arg_start_temperature);
  simulated_annealing_with_reheating_arguments.AddArgument(this->arg_min_temperature);
  simulated_annealing_with_reheating_arguments.AddArgument(this->arg_cooling_rate);
  simulated_annealing_with_reheating_arguments.AddArgument(this->arg_neighbors_sampled);
  simulated_annealing_with_reheating_arguments.AddArgument(this->arg_neighbors_accepted);
  simulated_annealing_with_reheating_arguments.AddArgument(arg_reheat);
  simulated_annealing_with_reheating_arguments.AddArgument(arg_first_reheat);
  simulated_annealing_with_reheating_arguments.AddArgument(arg_first_reheat);
  simulated_annealing_with_reheating_arguments.AddArgument(arg_max_reheats);
  
  cl.AddArgument(simulated_annealing_with_reheating_arguments);
  cl.MatchArgument(simulated_annealing_with_reheating_arguments);
  if (simulated_annealing_with_reheating_arguments.IsSet())
  {
    if (this->arg_start_temperature.IsSet())
      this->start_temperature = this->arg_start_temperature.GetValue();
    if (this->arg_min_temperature.IsSet())
      this->min_temperature = this->arg_min_temperature.GetValue();
    this->cooling_rate = this->arg_cooling_rate.GetValue();
    if (arg_reheat.IsSet())
      reheat = arg_reheat.GetValue();
    if (arg_first_reheat.IsSet())
      first_reheat = arg_first_reheat.GetValue();
    else
      first_reheat = reheat;
    this->max_neighbors_sampled = this->arg_neighbors_sampled.GetValue();
    if (this->arg_neighbors_accepted.IsSet())
      this->max_neighbors_accepted = this->arg_neighbors_accepted.GetValue();
    if (arg_max_reheats.IsSet())
      max_reheats = arg_max_reheats.GetValue();
  }
}

template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealingWithReheating<Input,State,Move,CFtype>::Print(std::ostream& os) const
{
  os  << "Simulated Annealing With Reheating Runner: " << std::endl;
  os  << "  Max iterations: " << this->max_iterations << std::endl;
  os  << "  Start temperature: " << this->start_temperature << std::endl;
  os  << "  Min temperature: " << this->min_temperature << std::endl;
  os  << "  Cooling rate: " << this->cooling_rate << std::endl;
  os  << "  Reheat ratio: " << reheat << std::endl;
  os  << "  First reheat ratio: " << first_reheat << std::endl;
  os  << "  First Descent Iterations ratio: " << first_descent_iterations_ratio << std::endl;
  os  << "  Number of reheats: " << max_reheats << std::endl;
}

template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealingWithReheating<Input,State,Move,CFtype>::InitializeRun()
{
  SimulatedAnnealing<Input,State,Move,CFtype>::InitializeRun();
  unsigned number_of_temperatures = -log(this->start_temperature/this->min_temperature) / log(this->cooling_rate);
  this->max_neighbors_sampled = ceil((first_descent_iterations_ratio * this->max_iterations) /number_of_temperatures);
  this->max_neighbors_accepted = this->max_neighbors_sampled;
}

/**
 A move is randomly picked.
 */
template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealingWithReheating<Input,State,Move,CFtype>::CompleteMove()
{
  SimulatedAnnealing<Input,State,Move,CFtype>::CompleteMove();
  if (SimulatedAnnealing<Input,State,Move,CFtype>::StopCriterion() && reheats < max_reheats)
  {
    if (reheats == 0)
      this->start_temperature = this->start_temperature * first_reheat;
    else
      this->start_temperature = this->start_temperature * reheat;
    
    this->temperature = this->start_temperature;
    
    std::cerr << "Reheat " << this->temperature << std::endl;
    
    unsigned number_of_temperatures = -log(this->start_temperature / this->min_temperature) / log(this->cooling_rate);
    this->max_neighbors_sampled = ceil(((1.0 - first_descent_iterations_ratio) * this->max_iterations) / number_of_temperatures);
    this->max_neighbors_accepted = this->max_neighbors_sampled;
    reheats++;
  }
}

template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealingWithReheating<Input,State,Move,CFtype>::ReadParameters(std::istream& is, std::ostream& os)

{
  os << "SIMULATED ANNEALING WITH REHEATING -- INPUT PARAMETERS" << std::endl;
  os << "  Start temperature: ";
  is >> this->start_temperature;
  os << "  Min temperature: ";
  is >> this->min_temperature;
  os << "  Cooling rate: ";
  is >> this->cooling_rate;
  os << "  Max total iterations: ";
  is >> this->max_iterations;
  os << "  Number of reheats: ";
  is >> this->max_reheats;
  os << "  Reheat ratio: ";
  is >> reheat;
  os << "  First reheat ratio: ";
  is >> first_reheat;
  os << "  First Descent Iterations ratio: ";
  is >> first_descent_iterations_ratio;
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
