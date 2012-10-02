#if !defined(_HILL_CLIMBING_HH_)
#define _HILL_CLIMBING_HH_

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
class HillClimbing : public MoveRunner<Input,State,Move,CFtype>
{
public:
  HillClimbing(const Input& in, StateManager<Input,State,CFtype>& e_sm,
               NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne, std::string name);

  // FIXME: to remove parameter accessors
  virtual void SetMaxIdleIterations(unsigned long int m) { max_idle_iterations = m; }
protected:
  bool MaxIdleIterationExpired() const;
  bool MaxIterationExpired() const;
  bool StopCriterion();
  bool AcceptableMove();
  void SelectMove();
  // parameters
  Parameter<unsigned long int> max_idle_iterations;
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
   @param cl a CLParser optional object with command line parameters
*/

template <class Input, class State, class Move, typename CFtype>
HillClimbing<Input,State,Move,CFtype>::HillClimbing(const Input& in,
                                                    StateManager<Input,State,CFtype>& e_sm,
                                                    NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                                                    std::string name)
: MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name, "Hill Climbing Runner"),
// parameters
max_idle_iterations("max_idle_iterations", "Total number of allowed idle iterations", this->parameters)
{
}

/**
   The select move strategy for the hill climbing simply looks for a
   random move.
*/
template <class Input, class State, class Move, typename CFtype>
void HillClimbing<Input,State,Move,CFtype>::SelectMove()
{
  this->ne.RandomMove(*this->p_current_state, this->current_move);
  this->current_move_cost = this->ne.DeltaCostFunction(*this->p_current_state, this->current_move);
}



template <class Input, class State, class Move, typename CFtype>
bool HillClimbing<Input,State,Move,CFtype>::MaxIdleIterationExpired() const
{
  return this->iteration - this->iteration_of_best >= this->max_idle_iterations;
}

template <class Input, class State, class Move, typename CFtype>
bool HillClimbing<Input,State,Move,CFtype>::MaxIterationExpired() const
{
  return this->iteration >= this->max_iterations;
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

#endif
