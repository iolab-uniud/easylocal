#if !defined(HILLCLIMBING_HH_)
#define HILLCLIMBING_HH_

#include <helpers/StateManager.hh>
#include <helpers/NeighborhoodExplorer.hh>
#include <runners/MoveRunner.hh>
#include <stdexcept>

/** The Hill Climbing runner considers random move selection. A move
    is then performed only if it does improve or it leaves unchanged
    the value of the cost function.  
    @ingroup Runners 
*/
template <class Input, class State, class Move, typename CFtype = int>
class HillClimbing
  : public MoveRunner<Input,State,Move,CFtype>
{
public:
  HillClimbing(const Input& in, StateManager<Input,State,CFtype>& e_sm,
	       NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne, std::string name);
  HillClimbing(const Input& in, StateManager<Input,State,CFtype>& e_sm,
	       NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne, std::string name, CLParser& cl);
  void Print(std::ostream& os = std::cout) const;
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
  virtual void SetMaxIdleIteration(unsigned long m) { max_idle_iteration = m; }
  bool MaxIdleIterationExpired() const;
protected:
  void GoCheck() const;
  void InitializeRun();
  void TerminateRun();
  bool StopCriterion();
  bool AcceptableMove();
  void StoreMove();
  void SelectMove();
  // parameters
  unsigned long max_idle_iteration;
  ArgumentGroup hill_climbing_arguments;
  ValArgument<unsigned long> arg_max_idle_iteration;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
   Constructs a hill climbing runner by linking it to a state manager, 
   a neighborhood explorer, and an input object.

   @param s a pointer to a compatible state manager
   @param ne a pointer to a compatible neighborhood explorer
   @param in a poiter to an input object
*/
template <class Input, class State, class Move, typename CFtype>
HillClimbing<Input,State,Move,CFtype>::HillClimbing(const Input& in,
                                                    StateManager<Input,State,CFtype>& e_sm,
                                                    NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                                                    std::string name)
  : MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name), max_idle_iteration(0),
    hill_climbing_arguments("hc_" + name, "hc_" + name, false), arg_max_idle_iteration("max_idle_iteration", "mii", true)
{
  hill_climbing_arguments.AddArgument(arg_max_idle_iteration);
}

template <class Input, class State, class Move, typename CFtype>
HillClimbing<Input,State,Move,CFtype>::HillClimbing(const Input& in,
                                                    StateManager<Input,State,CFtype>& e_sm,
                                                    NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                                                    std::string name,
						    CLParser& cl)
  : MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name), max_idle_iteration(0),
    hill_climbing_arguments("hc_" + name, "hc_" + name, false), arg_max_idle_iteration("max_idle_iteration", "mii", true)
{
  hill_climbing_arguments.AddArgument(arg_max_idle_iteration);
  cl.AddArgument(hill_climbing_arguments);
  cl.MatchArgument(hill_climbing_arguments);
  if (hill_climbing_arguments.IsSet())
    max_idle_iteration = arg_max_idle_iteration.GetValue();
}

template <class Input, class State, class Move, typename CFtype>
void HillClimbing<Input,State,Move,CFtype>::Print(std::ostream& os) const
{
  os  << "Hill Climbing Runner: " << this->name << std::endl;
  os  << "  Max iterations: " << this->max_iteration << std::endl;
  os  << "  Max idle iteration: " << this->max_idle_iteration << std::endl;
}

/**
   The select move strategy for the hill climbing simply looks for a
   random move.
*/
template <class Input, class State, class Move, typename CFtype>
void HillClimbing<Input,State,Move,CFtype>::SelectMove()
{
  this->ne.RandomMove(this->current_state, this->current_move);
  this->ComputeMoveCost();
}

/**
   The hill climbing initialization simply invokes 
   the superclass companion method.
*/
template <class Input, class State, class Move, typename CFtype>
void HillClimbing<Input,State,Move,CFtype>::InitializeRun()
{
  MoveRunner<Input,State,Move,CFtype>::InitializeRun();
}

template <class Input, class State, class Move, typename CFtype>
void HillClimbing<Input,State,Move,CFtype>::GoCheck() const

{
  if (this->max_idle_iteration == 0)
    throw std::logic_error("max_idle_iteration is zero for object " + this->name);
}

/**
   At the end of the run, the best state found is set with the last visited
   state (it is always a local minimum).
*/
template <class Input, class State, class Move, typename CFtype>
void HillClimbing<Input,State,Move,CFtype>::TerminateRun()
{
  MoveRunner<Input,State,Move,CFtype>::TerminateRun();
  this->best_state = this->current_state;
  this->best_state_cost = this->current_state_cost;
}

template <class Input, class State, class Move, typename CFtype>
bool HillClimbing<Input,State,Move,CFtype>::MaxIdleIterationExpired() const
{
  return this->number_of_iterations - this->iteration_of_best >= this->max_idle_iteration; 
}

/**
   The stop criterion is based on the number of iterations elapsed from
   the last strict improvement of the best state cost.
*/
template <class Input, class State, class Move, typename CFtype>
bool HillClimbing<Input,State,Move,CFtype>::StopCriterion()
{ 
  return MaxIdleIterationExpired() || this->MaxIterationExpired();
}

/**
   A move is accepted if it is non worsening (i.e., it improves the cost
   or leaves it unchanged).
*/
template <class Input, class State, class Move, typename CFtype>
bool HillClimbing<Input,State,Move,CFtype>::AcceptableMove()
{ return LessOrEqualThan(this->current_move_cost,(CFtype)0); }

/**
   The store move for hill climbing simply updates the variable that
   keeps track of the last improvement.
*/
template <class Input, class State, class Move, typename CFtype>
void HillClimbing<Input,State,Move,CFtype>::StoreMove()
{
  if (this->observer != NULL)
    this->observer->NotifyStoreMove(*this);
  if (LessThan(this->current_move_cost, (CFtype)0))
    {
      if (this->observer != NULL)
	this->observer->NotifyNewBest(*this);
      this->iteration_of_best = this->number_of_iterations;
      this->best_state_cost = this->current_state_cost;
    }
}

template <class Input, class State, class Move, typename CFtype>
void HillClimbing<Input,State,Move,CFtype>::ReadParameters(std::istream& is, std::ostream& os)

{
  os << "HILL CLIMBING -- INPUT PARAMETERS" << std::endl;
  os << "  Number of idle iterations: ";
  is >> this->max_idle_iteration;
}

#endif /*HILLCLIMBING_HH_*/
