#if !defined(_TABU_SEARCH_HH_)
#define _TABU_SEARCH_HH_

/** The Tabu Search runner explores a subset of the current
 neighborhood. Among the elements in it, the one that gives the
 minimum value of the cost function becomes the new current
 state, independently of the fact whether its value is less or
 greater than the current one.
 
 Such a choice allows the algorithm to escape from local minima,
 but creates the risk of cycling among a set of states.  In order to
 prevent cycling, the so-called tabu list is used, which
 determines the forbidden moves. This list stores the most recently
 accepted moves, and the inverses of the moves in the list are
 forbidden.  
 @ingroup Runners
 */

#include <runners/MoveRunner.hh>
#include <helpers/StateManager.hh>
#include <helpers/NeighborhoodExplorer.hh>
#include <helpers/TabuListManager.hh>
#include <stdexcept>

template <class Input, class State, class Move, typename CFtype>
class TabuSearch : public MoveRunner<Input,State,Move,CFtype>
{
public:
  TabuSearch(const Input& in, StateManager<Input,State,CFtype>& e_sm,
             NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
             TabuListManager<State,Move,CFtype>& e_tlm,
             std::string name);	
  std::string StatusString();
  
  virtual void Print(std::ostream& os = std::cout) const;
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
  virtual void SetMaxIdleIteration(unsigned long int m) { max_idle_iterations = m; }
  TabuListManager<State,Move,CFtype>& GetTabuListManager() { return pm; }
  bool MaxIdleIterationExpired() const;
protected:
  void InitializeRun() throw (ParameterNotSet, IncorrectParameterValue);
  bool StopCriterion();
  void SelectMove();
  bool AcceptableMove();
  void CompleteMove();
  TabuListManager<State,Move,CFtype>& pm; /**< A reference to a tabu list manger. */
  // parameters
  Parameter<unsigned long int> max_idle_iterations;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
 Constructs a tabu search runner by linking it to a state manager, 
 a neighborhood explorer, a tabu list manager, and an input object.
 
 @param s a pointer to a compatible state manager
 @param ne a pointer to a compatible neighborhood explorer
 @param tlm a pointer to a compatible tabu list manager
 @param in a poiter to an input object
 */

template <class Input, class State, class Move, typename CFtype>
TabuSearch<Input,State,Move,CFtype>::TabuSearch(const Input& in,
                                                StateManager<Input,State,CFtype>& e_sm,
                                                NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                                                TabuListManager<State,Move,CFtype>& tlm,
                                                std::string name)
: MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name, "Tabu Search Runner"), pm(tlm),
// parameters
max_idle_iterations("max_idle_iterations", "Maximum number of idle iterations", this->parameters)
{
}

template <class Input, class State, class Move, typename CFtype>
void TabuSearch<Input,State,Move,CFtype>::Print(std::ostream& os) const
{
  Runner<Input, State, CFtype>::Print(os);
  pm.Print(os);
}

/**
 Initializes the run by invoking the companion superclass method, and
 cleans the tabu list.
 */
template <class Input, class State, class Move, typename CFtype>
void TabuSearch<Input,State,Move,CFtype>::InitializeRun() throw (ParameterNotSet, IncorrectParameterValue)
{
  MoveRunner<Input,State,Move,CFtype>::InitializeRun();
  // pm.SetLength(min_tenure, max_tenure); not my responsibility
  pm.Clean();
}


/**
 Selects always the best move that is non prohibited by the tabu list 
 mechanism.
 */
template <class Input, class State, class Move, typename CFtype>
void TabuSearch<Input,State,Move,CFtype>::SelectMove()
{
  this->current_move_cost = this->ne.BestMove(*this->p_current_state, this->current_move, pm);
}

template <class Input, class State, class Move, typename CFtype>
bool TabuSearch<Input,State,Move,CFtype>::MaxIdleIterationExpired() const
{
  return this->iteration - this->iteration_of_best >= this->max_idle_iterations;
}

/**
 The stop criterion is based on the number of iterations elapsed from
 the last strict improvement of the best state cost.
 */
template <class Input, class State, class Move, typename CFtype>
bool TabuSearch<Input,State,Move,CFtype>::StopCriterion()
{ 
  return MaxIdleIterationExpired() || this->MaxIterationExpired();
}

/**
 In tabu search the selected move is always accepted.
 That is, the acceptability test is replaced by the 
 prohibition mechanism which is managed inside the selection.
 */
template <class Input, class State, class Move, typename CFtype>
bool TabuSearch<Input,State,Move,CFtype>::AcceptableMove()
{ 
  return true; 
}

/**
 Stores the move by inserting it in the tabu list, if the state obtained
 is better than the one found so far also the best state is updated.
 */
template <class Input, class State, class Move, typename CFtype>
void TabuSearch<Input,State,Move,CFtype>::CompleteMove()
{
  pm.InsertMove(*this->p_current_state, this->current_move, this->current_move_cost,
                this->current_state_cost, this->best_state_cost);
}

template <class Input, class State, class Move, typename CFtype>
void TabuSearch<Input,State,Move,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
{
  Runner<Input, State, CFtype>::ReadParameters(is, os);
  pm.ReadParameters(is, os);
}

/**
 Create a string containing the status of the runner
 */
template <class Input, class State, class Move, typename CFtype>
std::string TabuSearch<Input,State,Move,CFtype>::StatusString()
{
  std::stringstream status;
  status << "["
	 << "TL length = " << pm.ListLength() << " (" << pm.MinTenure() << "-" << pm.MaxTenure() << ")"
	   << "]";
  return status.str();
}

#endif // _TABU_SEARCH_HH_
