#if !defined(_GREAT_DELUGE_HH_)
#define _GREAT_DELUGE_HH_

#include <runners/MoveRunner.hh>
#include <helpers/StateManager.hh>
#include <helpers/NeighborhoodExplorer.hh>
#include <stdexcept>

/** The Great Deluge runner relies on a probabilistic local
 search technique whose name comes from ... the Bible?
 
 At each iteration a candidate move is generated at random, and
 it is always accepted if it is an improving move.  Instead, if
 the move is a worsening one, the new solution is accepted ...
 
 @ingroup Runners
 */
template <class Input, class State, class Move, typename CFtype = int>
class GreatDeluge : public MoveRunner<Input,State,Move,CFtype>
{
public:
  
  GreatDeluge(const Input& in,
              StateManager<Input,State,CFtype>& e_sm,
              NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
              std::string name,
              CLParser& cl = CLParser::empty);	
  
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
  void Print(std::ostream& os = std::cout) const;
  void SetLevelRate(double lr)  { level_rate = lr; }
  void SetMinLevel(double ml)  { min_level = ml; }
  void SetNeighborsSampled(unsigned int ns)  { neighbors_sampled = ns; }
  
protected:
  void InitializeRun();
  bool StopCriterion();
  void UpdateIterationCounter();
  void SelectMove();
  bool AcceptableMove();
  
  // parameters
  double level; /**< The current level. */
  double initial_level; /**< The initial level. */
  double min_level; /**< The minimum level. */
  double level_rate; /**< The level decreasing rate. */
  unsigned int neighbors_sampled;
  ArgumentGroup great_deluge_arguments;
  ValArgument<unsigned int> arg_neighbors_sampled;
  ValArgument<double> arg_level_rate;
  ValArgument<double> arg_initial_level_ratio;
  ValArgument<double> arg_min_level_ratio;
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
GreatDeluge<Input,State,Move,CFtype>::GreatDeluge(const Input& in,
                                                  StateManager<Input,State,CFtype>& e_sm,
                                                  NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                                                  std::string name,
                                                  CLParser& cl)
: MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name),
initial_level(1.15), min_level(0.9), level_rate(0.99), neighbors_sampled(1000),
great_deluge_arguments("gd_" + name, "gd_" + name, false), 
arg_neighbors_sampled("neighbors_sampled", "ns", true), arg_level_rate("level_rate", "lr", false),
arg_initial_level_ratio("intial_level_ratio", "ilr", false, 1.15), arg_min_level_ratio("min_level_ratio", "mlr", false, 0.9)
{
  great_deluge_arguments.AddArgument(arg_level_rate);
  great_deluge_arguments.AddArgument(arg_neighbors_sampled);
  great_deluge_arguments.AddArgument(arg_initial_level_ratio);
  great_deluge_arguments.AddArgument(arg_min_level_ratio);
  cl.AddArgument(great_deluge_arguments);
  cl.MatchArgument(great_deluge_arguments);
  if (great_deluge_arguments.IsSet())
  {
    if (arg_level_rate.IsSet())
      level_rate = arg_level_rate.GetValue();
    if (arg_neighbors_sampled.IsSet())
      neighbors_sampled = arg_neighbors_sampled.GetValue();
    if (arg_initial_level_ratio.IsSet())
      initial_level = arg_initial_level_ratio.GetValue();
    if (arg_min_level_ratio.IsSet())
      min_level = arg_min_level_ratio.GetValue();
  }
}

template <class Input, class State, class Move, typename CFtype>
void GreatDeluge<Input,State,Move,CFtype>::Print(std::ostream& os) const
{
  os  << "Great Deluge Runner: " << std::endl;
  os  << "  Neighbors sampled: " << neighbors_sampled << std::endl;
  os  << "  Level rate: " << level_rate << std::endl;
}

/**
 Initializes the run by invoking the companion superclass method, and
 setting the temperature to the start value.
 */
template <class Input, class State, class Move, typename CFtype>
void GreatDeluge<Input,State,Move,CFtype>::InitializeRun()
{
  MoveRunner<Input,State,Move,CFtype>::InitializeRun();
  level = initial_level * this->current_state_cost;
}

/**
 A move is randomly picked and its cost is stored.
 */
template <class Input, class State, class Move, typename CFtype>
void GreatDeluge<Input,State,Move,CFtype>::SelectMove()
{
  this->ne.RandomMove(this->current_state, this->current_move);
  this->current_move_cost = this->ne.DeltaCostFunction(this->current_state, this->current_move);
}

template <class Input, class State, class Move, typename CFtype>
void GreatDeluge<Input,State,Move,CFtype>::ReadParameters(std::istream& is, std::ostream& os)

{
  os << "GREAT DELUGE -- INPUT PARAMETERS" << std::endl;
  os << "  Level rate: ";
  is >> level_rate;
  os << "  Neighbors sampled: ";
  is >> this->neighbors_sampled;
}

/**
 The search stops when a low temperature has reached.
 */
template <class Input, class State, class Move, typename CFtype>
bool GreatDeluge<Input,State,Move,CFtype>::StopCriterion()
{ return level < min_level * this->best_state_cost; }

/**
 At regular steps, the temperature is decreased 
 multiplying it by a cooling rate.
 */
template <class Input, class State, class Move, typename CFtype>
void GreatDeluge<Input,State,Move,CFtype>::UpdateIterationCounter()
{
  MoveRunner<Input,State,Move,CFtype>::UpdateIterationCounter();
  if (this->number_of_iterations % neighbors_sampled == 0)
  {
    level *= level_rate;
  }
}

/** A move is surely accepted if it improves the cost function
 or with exponentially decreasing probability if it is 
 a worsening one.
 */
template <class Input, class State, class Move, typename CFtype>
bool GreatDeluge<Input,State,Move,CFtype>::AcceptableMove()
{ 
  return LessOrEqualThan(this->current_move_cost,(CFtype)0)
  || LessOrEqualThan((double)(this->current_move_cost + this->current_state_cost),level); 
}

#endif // _GREAT_DELUGE_HH_
