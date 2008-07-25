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

#if !defined(_GREAT_DELUGE_HH_)
#define _GREAT_DELUGE_HH_

#include <runners/MoveRunner.hh>
#include <helpers/StateManager.hh>
#include <helpers/NeighborhoodExplorer.hh>
#include <stdexcept>

/** The Great Deluge runner relies on a probabilistic local
    search technique whose name comes from...

    At each iteration a candidate move is generated at random, and
    it is always accepted if it is an improving move.  Instead, if
    the move is a worsening one, the new solution is accepted ...

    @ingroup Runners
*/
template <class Input, class State, class Move, typename CFtype = int>
class GreatDeluge
  : public MoveRunner<Input,State,Move,CFtype>
{
public:
  GreatDeluge(const Input& in,
	      StateManager<Input,State,CFtype>& e_sm,
	      NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
	      std::string name);
  GreatDeluge(const Input& in,
	      StateManager<Input,State,CFtype>& e_sm,
	      NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
	      std::string name,
	      CLParser& cl);	
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
  void Print(std::ostream& os = std::cout) const;
  void SetLevelRate(double lr)  { level_rate = lr; }
  virtual void SetMaxIdleIteration(unsigned long m) { max_idle_iteration = m; }


protected:
  void GoCheck() const;
  void InitializeRun();
  bool StopCriterion();
  void UpdateIterationCounter();
  void SelectMove();
  bool AcceptableMove();
  void StoreMove();

  // parameters
  double level; /**< The current level. */
  double min_level; /**< The minimum level. */
  double level_rate; /**< The level decreasing rate. */
  unsigned long max_idle_iteration;
  ArgumentGroup great_deluge_arguments;
  ValArgument<unsigned> arg_max_idle_iteration;
  ValArgument<double> arg_level_rate;
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
  : MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name),
    min_level(0.001), level_rate(0.9999), 
    great_deluge_arguments("sa_" + name, "sa_" + name, false), 
    arg_max_idle_iteration("max_idle_iteration", "mii", true), arg_level_rate("level_rate", "lr", false)
{
  great_deluge_arguments.AddArgument(arg_level_rate);
  great_deluge_arguments.AddArgument(arg_max_idle_iteration);
}

template <class Input, class State, class Move, typename CFtype>
GreatDeluge<Input,State,Move,CFtype>::GreatDeluge(const Input& in,
						  StateManager<Input,State,CFtype>& e_sm,
						  NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
						  std::string name,
						  CLParser& cl)
  : MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name),
    min_level(0.001), level_rate(0.9999), 
    great_deluge_arguments("sa_" + name, "sa_" + name, false), 
    arg_max_idle_iteration("max_idle_iteration", "mii", true), arg_level_rate("level_rate", "lr", false)
{
  great_deluge_arguments.AddArgument(arg_level_rate);
  great_deluge_arguments.AddArgument(arg_max_idle_iteration);
  cl.AddArgument(great_deluge_arguments);
  cl.MatchArgument(great_deluge_arguments);
  if (great_deluge_arguments.IsSet())
    {
      if (arg_level_rate.IsSet())
	level_rate = arg_level_rate.GetValue();
      if (arg_max_idle_iteration.IsSet())
	max_idle_iteration = arg_max_idle_iteration.GetValue();
    }
}



template <class Input, class State, class Move, typename CFtype>
void GreatDeluge<Input,State,Move,CFtype>::Print(std::ostream& os) const
{
  os  << "Great Deluge Runner: " << std::endl;
  os  << "  Max idle iteration: " << max_idle_iteration << std::endl;
  os  << "  Level rate: " << level_rate << std::endl;
}

template <class Input, class State, class Move, typename CFtype>
void GreatDeluge<Input,State,Move,CFtype>::GoCheck() const

{
  if (level_rate <= 0)
    throw std::logic_error("negative level_rate for object " + this->name)
      ;
  if (max_idle_iteration == 0)
    throw std::logic_error("max_idle_iteration is zero for object " + this->name);
}

/**
   Initializes the run by invoking the companion superclass method, and
   setting the temperature to the start value.
*/
template <class Input, class State, class Move, typename CFtype>
void GreatDeluge<Input,State,Move,CFtype>::InitializeRun()
{
  MoveRunner<Input,State,Move,CFtype>::InitializeRun();
  level = static_cast<double>(this->current_state_cost);
}

/**
   A move is randomly picked.
*/
template <class Input, class State, class Move, typename CFtype>
void GreatDeluge<Input,State,Move,CFtype>::SelectMove()
{
  this->ne.RandomMove(this->current_state, this->current_move);
  this->ComputeMoveCost();
}

/**
   A move is randomly picked.
*/
template <class Input, class State, class Move, typename CFtype>
void GreatDeluge<Input,State,Move,CFtype>::StoreMove()
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
}

template <class Input, class State, class Move, typename CFtype>
void GreatDeluge<Input,State,Move,CFtype>::ReadParameters(std::istream& is, std::ostream& os)

{
  os << "GREAT DELUGE -- INPUT PARAMETERS" << std::endl;
  os << "  Level rate: ";
  is >> level_rate;
  os << "  Max number of idle iterations: ";
  is >> this->max_idle_iteration;
}

/**
   The search stops when a low temperature has reached.
*/
template <class Input, class State, class Move, typename CFtype>
bool GreatDeluge<Input,State,Move,CFtype>::StopCriterion()
{ return (this->number_of_iterations - this->iteration_of_best >= this->max_idle_iteration) && level < min_level; }

/**
   At regular steps, the temperature is decreased 
   multiplying it by a cooling rate.
*/
template <class Input, class State, class Move, typename CFtype>
void GreatDeluge<Input,State,Move,CFtype>::UpdateIterationCounter()
{
  MoveRunner<Input,State,Move,CFtype>::UpdateIterationCounter();
  level *= level_rate;
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

#endif // define _GREAT_DELUGE_HH_
