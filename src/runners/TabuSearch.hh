// $Id$
// This file is part of EasyLocalpp: a C++ Object-Oriented framework
// aimed at easing the development of Local Search algorithms.
// Copyright (C) 2001--2008 Andrea Schaerf, Luca Di Gaspero. 
//
// EasyLocalpp is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// EasyLocalpp is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with EasyLocalpp. If not, see <http://www.gnu.org/licenses/>.

#if !defined(TABUSEARCH_HH_)
#define TABUSEARCH_HH_

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

template <class Input, class State, class Move, typename CFtype = int>
class TabuSearch
: public MoveRunner<Input,State,Move,CFtype>
{
public:
  TabuSearch(const Input& in, StateManager<Input,State,CFtype>& e_sm,
           NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
           TabuListManager<State,Move,CFtype>& e_tlm,
           std::string name);
  TabuSearch(const Input& in, StateManager<Input,State,CFtype>& e_sm,
           NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
           TabuListManager<State,Move,CFtype>& e_tlm,
           std::string name, CLParser& cl);	
  
  TabuSearch(const Input& in, StateManager<Input,State,CFtype>& e_sm,
             NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
             TabuListManager<State,Move,CFtype>& e_tlm,
             std::string name, AbstractTester<Input,State,CFtype>& t);
  TabuSearch(const Input& in, StateManager<Input,State,CFtype>& e_sm,
             NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
             TabuListManager<State,Move,CFtype>& e_tlm,
             std::string name, CLParser& cl, AbstractTester<Input,State,CFtype>& t);	  
void Print(std::ostream& os = std::cout) const;
void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
virtual void SetMaxIdleIteration(unsigned long m) { max_idle_iteration = m; }
TabuListManager<State,Move,CFtype>& GetTabuListManager() { return pm; }
bool MaxIdleIterationExpired() const;
protected:
void GoCheck() const;
void InitializeRun();
bool StopCriterion();
void SelectMove();
bool AcceptableMove();
void StoreMove();
TabuListManager<State,Move,CFtype>& pm; /**< A reference to a tabu list manger. */
// parameters
unsigned long max_idle_iteration;
ArgumentGroup tabu_search_arguments;
ValArgument<unsigned long> arg_max_idle_iteration;
ValArgument<unsigned int, 2> arg_tabu_tenure;
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
: MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name), pm(tlm), max_idle_iteration(0), 
tabu_search_arguments("ts_" + name, "ts_" + name, false), arg_max_idle_iteration("max_idle_iteration", "mii", true), arg_tabu_tenure("tabu_tenure", "tt", true)
{
  tabu_search_arguments.AddArgument(arg_max_idle_iteration);
  tabu_search_arguments.AddArgument(arg_tabu_tenure);
}

template <class Input, class State, class Move, typename CFtype>
TabuSearch<Input,State,Move,CFtype>::TabuSearch(const Input& in,
                                                StateManager<Input,State,CFtype>& e_sm,
                                                NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                                                TabuListManager<State,Move,CFtype>& tlm,
                                                std::string name, 
                                                CLParser& cl)
: MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name), pm(tlm), max_idle_iteration(0), 
tabu_search_arguments("ts_" + name, "ts_" + name, false), arg_max_idle_iteration("max_idle_iteration", "mii", true), arg_tabu_tenure("tabu_tenure", "tt", true)
{
  tabu_search_arguments.AddArgument(arg_max_idle_iteration);
  tabu_search_arguments.AddArgument(arg_tabu_tenure);
  cl.AddArgument(tabu_search_arguments);
  cl.MatchArgument(tabu_search_arguments);
  if (tabu_search_arguments.IsSet())
  {
    pm.SetLength(arg_tabu_tenure.GetValue(0), arg_tabu_tenure.GetValue(1));
    max_idle_iteration = arg_max_idle_iteration.GetValue();
  }
}

template <class Input, class State, class Move, typename CFtype>
TabuSearch<Input,State,Move,CFtype>::TabuSearch(const Input& in,
                                                StateManager<Input,State,CFtype>& e_sm,
                                                NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                                                TabuListManager<State,Move,CFtype>& tlm,
                                                std::string name, AbstractTester<Input,State,CFtype>& t)
: MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name, t), pm(tlm), max_idle_iteration(0), 
tabu_search_arguments("ts_" + name, "ts_" + name, false), arg_max_idle_iteration("max_idle_iteration", "mii", true), arg_tabu_tenure("tabu_tenure", "tt", true)
{
  tabu_search_arguments.AddArgument(arg_max_idle_iteration);
  tabu_search_arguments.AddArgument(arg_tabu_tenure);
}

template <class Input, class State, class Move, typename CFtype>
TabuSearch<Input,State,Move,CFtype>::TabuSearch(const Input& in,
                                                StateManager<Input,State,CFtype>& e_sm,
                                                NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                                                TabuListManager<State,Move,CFtype>& tlm,
                                                std::string name, 
                                                CLParser& cl, AbstractTester<Input,State,CFtype>& t)
: MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name, t), pm(tlm), max_idle_iteration(0), 
tabu_search_arguments("ts_" + name, "ts_" + name, false), arg_max_idle_iteration("max_idle_iteration", "mii", true), arg_tabu_tenure("tabu_tenure", "tt", true)
{
  tabu_search_arguments.AddArgument(arg_max_idle_iteration);
  tabu_search_arguments.AddArgument(arg_tabu_tenure);
  cl.AddArgument(tabu_search_arguments);
  cl.MatchArgument(tabu_search_arguments);
  if (tabu_search_arguments.IsSet())
  {
    pm.SetLength(arg_tabu_tenure.GetValue(0), arg_tabu_tenure.GetValue(1));
    max_idle_iteration = arg_max_idle_iteration.GetValue();
  }
}

template <class Input, class State, class Move, typename CFtype>
void TabuSearch<Input,State,Move,CFtype>::Print(std::ostream& os) const
{
  os  << "Tabu Search Runner: " << this->name << std::endl;
  os  << "  Max iterations: " << this->max_iteration << std::endl;
  os  << "  Max idle iteration: " << max_idle_iteration << std::endl;
  pm.Print(os);
}

/**
 Initializes the run by invoking the companion superclass method, and
 cleans the tabu list.
 */
template <class Input, class State, class Move, typename CFtype>
void TabuSearch<Input,State,Move,CFtype>::InitializeRun()
{
  MoveRunner<Input,State,Move,CFtype>::InitializeRun();
  pm.Clean();
}

template <class Input, class State, class Move, typename CFtype>
void TabuSearch<Input,State,Move,CFtype>::GoCheck() const

{
  if (this->max_idle_iteration == 0)
    throw std::logic_error("max_idle_iteration is zero for object " + this->name);
}


/**
 Selects always the best move that is non prohibited by the tabu list 
 mechanism.
 */
template <class Input, class State, class Move, typename CFtype>
void TabuSearch<Input,State,Move,CFtype>::SelectMove()
{
  this->current_move_cost = this->ne.BestMove(this->current_state, this->current_move, pm);  
}

template <class Input, class State, class Move, typename CFtype>
bool TabuSearch<Input,State,Move,CFtype>::MaxIdleIterationExpired() const
{
  return this->number_of_iterations - this->iteration_of_best >= this->max_idle_iteration; 
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
void TabuSearch<Input,State,Move,CFtype>::StoreMove()
{
  if (this->observer != NULL)
    this->observer->NotifyStoreMove(*this);
  pm.InsertMove(this->current_state, this->current_move, this->current_move_cost,
                this->current_state_cost, this->best_state_cost);
  if (LessOrEqualThan(this->current_state_cost,this->best_state_cost))
  { // same cost states are accepted as best for diversification
    if (LessThan(this->current_state_cost,this->best_state_cost))
    {
      if (this->observer != NULL)
        this->observer->NotifyNewBest(*this);
      this->iteration_of_best = this->number_of_iterations;
      this->best_state_cost = this->current_state_cost;
    }
    this->best_state = this->current_state;
  }
}

template <class Input, class State, class Move, typename CFtype>
void TabuSearch<Input,State,Move,CFtype>::ReadParameters(std::istream& is, std::ostream& os)

{
  os << "TABU SEARCH -- INPUT PARAMETERS" << std::endl;
  pm.ReadParameters(is, os);
  os << "  Number of idle iterations: ";
  is >> max_idle_iteration;
}
#endif /*TABUSEARCH_HH_*/
