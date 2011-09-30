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
  GreatDeluge(const Input& in,
              StateManager<Input,State,CFtype>& e_sm,
              NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
              std::string name, AbstractTester<Input,State,CFtype>& t);
  GreatDeluge(const Input& in,
              StateManager<Input,State,CFtype>& e_sm,
              NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
              std::string name,
              CLParser& cl, AbstractTester<Input,State,CFtype>& t);
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
  void Print(std::ostream& os = std::cout) const;
  void SetLevelRate(double lr)  { level_rate = lr; }
  void SetMinLevel(double ml)  { min_level = ml; }
  void SetNeighborsSampled(unsigned int ns)  { neighbors_sampled = ns; }

protected:
  void GoCheck() const;
  void InitializeRun(bool first_round = true);
  bool StopCriterion();
  void UpdateIterationCounter();
  void SelectMove();
  bool AcceptableMove();
  void StoreMove();

  // parameters
  double level; /**< The current level. */
  double initial_level; /**< The initial level. */
  double min_level; /**< The minimum level. */
  double level_rate; /**< The level decreasing rate. */
  unsigned int neighbors_sampled;
  ArgumentGroup great_deluge_arguments;
  ValArgument<unsigned> arg_neighbors_sampled;
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
						  std::string name)
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
}

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
GreatDeluge<Input,State,Move,CFtype>::GreatDeluge(const Input& in,
                                                  StateManager<Input,State,CFtype>& e_sm,
                                                  NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                                                  std::string name, AbstractTester<Input,State,CFtype>& t)
: MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name, t),
initial_level(1.15), min_level(0.9), level_rate(0.99), neighbors_sampled(1000),
great_deluge_arguments("gd_" + name, "gd_" + name, false), 
arg_neighbors_sampled("neighbors_sampled", "ns", true), arg_level_rate("level_rate", "lr", false), 
arg_initial_level_ratio("intial_level_ratio", "ilr", false, 1.15), arg_min_level_ratio("min_level_ratio", "mlr", false, 0.9)
{
  great_deluge_arguments.AddArgument(arg_level_rate);
  great_deluge_arguments.AddArgument(arg_neighbors_sampled);
  great_deluge_arguments.AddArgument(arg_initial_level_ratio);
  great_deluge_arguments.AddArgument(arg_min_level_ratio);  
}

template <class Input, class State, class Move, typename CFtype>
GreatDeluge<Input,State,Move,CFtype>::GreatDeluge(const Input& in,
                                                  StateManager<Input,State,CFtype>& e_sm,
                                                  NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                                                  std::string name,
                                                  CLParser& cl, AbstractTester<Input,State,CFtype>& t)
: MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name, t),
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

template <class Input, class State, class Move, typename CFtype>
void GreatDeluge<Input,State,Move,CFtype>::GoCheck() const

{
  if (level_rate <= 0)
    throw std::logic_error("negative level_rate for object " + this->name)
      ;
  if (neighbors_sampled == 0)
    throw std::logic_error("neighbors_sampled is zero for object " + this->name);
}

/**
   Initializes the run by invoking the companion superclass method, and
   setting the temperature to the start value.
*/
template <class Input, class State, class Move, typename CFtype>
void GreatDeluge<Input,State,Move,CFtype>::InitializeRun(bool first_round)
{
  MoveRunner<Input,State,Move,CFtype>::InitializeRun();
  level = initial_level * this->current_state_cost;
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

#endif // define _GREAT_DELUGE_HH_
