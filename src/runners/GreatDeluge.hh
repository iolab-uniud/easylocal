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
              std::string name);	
  
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
  Parameter<double> initial_level; /**< The initial level. */
  Parameter<double> min_level; /**< The minimum level. */
  Parameter<double> level_rate; /**< The level decreasing rate. */
  Parameter<unsigned int> neighbors_sampled; /**< The number of neighbos sampled. */
  // state
  double level; /**< The current level. */
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
                                                  std::string name)
: MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name, "Great Deluge"),
// parameters
initial_level("initial_level", "FIXME", this->parameters),
min_level("min_level", "FIXME", this->parameters),
level_rate("level_rate", "FIXME", this->parameters),
neighbors_sampled("neighbors_sampled", "FIXME", this->parameters)
{

}

/**
 Initializes the run by invoking the companion superclass method, and
 setting current level to the initial one.
 */
template <class Input, class State, class Move, typename CFtype>
void GreatDeluge<Input,State,Move,CFtype>::InitializeRun()
{
  // FIXME: perform all other meaningful parameter checks (now they are not verified properly)
  MoveRunner<Input,State,Move,CFtype>::InitializeRun();
  if (initial_level <= 0.0)
    throw IncorrectParameterValue(initial_level, "should be greater than zero");
  if (min_level <= 0.0)
    throw IncorrectParameterValue(initial_level, "should be greater than zero");
  if (level_rate <= 0.0 || level_rate >= 1.0)
    throw IncorrectParameterValue(initial_level, "should be in the interval ]0, 1[");  
  
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
