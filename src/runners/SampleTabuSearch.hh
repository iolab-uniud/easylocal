// $Id$
// This file is part of EasyLocalpp: a C++ Object-Oriented framework
// aimed at easing the development of Local Search algorithms.
// Copyright (C) 2001--2011 Andrea Schaerf, Luca Di Gaspero. 
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

#if !defined(SAMPLETABUSEARCH_HH_)
#define SAMPLETABUSEARCH_HH_

/** The Sample Tabu Search runner explores a subset of the current
    neighborhood. Among the elements in it, the one that gives the
    minimum value of the cost function becomes the new current
    state, independently of the fact whether its value is less or
    greater than the current one. The neighborhood is sampled according 
    to a uniform distribution.
    
    Such a choice allows the algorithm to escape from local minima,
    but creates the risk of cycling among a set of states.  In order to
    prevent cycling, the so-called tabu list is used, which
    determines the forbidden moves. This list stores the most recently
    accepted moves, and the inverses of the moves in the list are
    forbidden.  
    @ingroup Runners
*/

#include <MoveRunner.hh>
#include <basics/std::logic_exception.hh>
#include <helpers/StateManager.hh>
#include <helpers/NeighborhoodExplorer.hh>
#include <helpers/TabuListManager.hh>
#include <TabuSearch.hh>

template <class Input, class State, class Move, typename CFtype = int>
class SampleTabuSearch
            : public TabuSearch<Input,State, Move>
{
public:
	void Print(std::ostream& os = std::cout) const;
	void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
	void SetSampleSize(unsigned int s)
	{ sample_size = s; }
	SampleTabuSearch(const Input& in, StateManager<Input,State,CFtype>& s,
									 NeighborhoodExplorer<Input,State,Move>& ne,
									 TabuListManager<State,Move,CFtype>& tlm, std::string name);
	SampleTabuSearch(const Input& in, StateManager<Input,State,CFtype>& s,
									 NeighborhoodExplorer<Input,State,Move>& ne,
									 TabuListManager<State,Move,CFtype>& tlm, std::string name, CLParser& cl);
  SampleTabuSearch(const Input& in, StateManager<Input,State,CFtype>& s,
									 NeighborhoodExplorer<Input,State,Move>& ne,
									 TabuListManager<State,Move,CFtype>& tlm, std::string name,
                   Tester<Input,Output,State>& t);
	SampleTabuSearch(const Input& in, StateManager<Input,State,CFtype>& s,
									 NeighborhoodExplorer<Input,State,Move>& ne,
									 TabuListManager<State,Move,CFtype>& tlm, std::string name, CLParser& cl,
                   Tester<Input,Output,State>& t);
protected:
	void SelectMove();
	unsigned int sample_size;
	ValArgument<unsigned int> arg_sample_size;
};

/*************************************************************************
 * Implementation
 *************************************************************************/


template <class Input, class State, class Move, typename CFtype = int>
SampleTabuSearch<Input,State,Move>::SampleTabuSearch(const Input& in, StateManager<Input,State,CFtype>& sm,
																										 NeighborhoodExplorer<Input,State,Move>& ne,
																										 TabuListManager<State,Move,CFtype>& tlm,
																										 std::string name)
: TabuSearch<Input,State,Move>(sm, ne, tlm, name), arg_sample_size("sample_size", "ss", true)
{
	this->tabu_search_arguments.AddArgument(arg_sample_size);
}

template <class Input, class State, class Move, typename CFtype = int>
SampleTabuSearch<Input,State,Move>::SampleTabuSearch(const Input& in, StateManager<Input,State,CFtype>& sm,
																										 NeighborhoodExplorer<Input,State,Move>& ne,
																										 TabuListManager<State,Move,CFtype>& tlm,
																										 std::string name,
																										 CLParser& cl)
: TabuSearch<Input,State,Move>(sm, ne, tlm, name, cl), arg_sample_size("sample_size", "ss", true)
{
	this->tabu_search_arguments.AddArgument(arg_sample_size);
	cl.MatchArgument(this->tabu_search_arguments);
	if (this->tabu_search_arguments.IsSet())
		sample_size = arg_sample_size.GetValue();
}


template <class Input, class State, class Move, typename CFtype = int>
SampleTabuSearch<Input,State,Move>::SampleTabuSearch(const Input& in, StateManager<Input,State,CFtype>& sm,
																										 NeighborhoodExplorer<Input,State,Move>& ne,
																										 TabuListManager<State,Move,CFtype>& tlm,
																										 std::string name,
                                                     Tester<Input,Output,State,CFtype>& t)
: TabuSearch<Input,State,Move>(sm, ne, tlm, name, t), arg_sample_size("sample_size", "ss", true)
{
	this->tabu_search_arguments.AddArgument(arg_sample_size);
}

template <class Input, class State, class Move, typename CFtype = int>
SampleTabuSearch<Input,State,Move>::SampleTabuSearch(const Input& in, StateManager<Input,State,CFtype>& sm,
																										 NeighborhoodExplorer<Input,State,Move>& ne,
																										 TabuListManager<State,Move,CFtype>& tlm,
																										 std::string name,
																										 CLParser& cl,
                                                     Tester<Input,Output,State,CFtype>& t)
: TabuSearch<Input,State,Move>(sm, ne, tlm, name, cl, t), arg_sample_size("sample_size", "ss", true)
{
	this->tabu_search_arguments.AddArgument(arg_sample_size);
	cl.MatchArgument(this->tabu_search_arguments);
	if (this->tabu_search_arguments.IsSet())
		sample_size = arg_sample_size.GetValue();
}


template <class Input, class State, class Move, typename CFtype = int>
void SampleTabuSearch<Input,State,Move>::Print(std::ostream& os) const
{
    TabuSearch<Input,State,Move>::Print(os);
    os << "Sample size: " << sample_size;
}

/**
    Selects always the best move that is non prohibited by the tabu list 
    mechanism.
*/
template <class Input, class State, class Move, typename CFtype = int>
void SampleTabuSearch<Input,State,Move>::SelectMove()
{
    this->current_move_cost = this->ne.SampleMove(this->current_state, this->current_move, sample_size, &this->pm);
}

template <class Input, class State, class Move, typename CFtype>
void SampleTabuSearch<Input,State,Move,CFtype>::ReadParameters(std::istream& is, std::ostream& os)

{
  os << "SAMPLE TABU SEARCH -- INPUT PARAMETERS" << std::endl;
  pm.ReadParameters(is, os);
  os << "  Number of idle iterations: ";
  is >> this->max_idle_iteration;
	os << "  Sample size: ";
	is >> sample_size;
}

#endif /*SAMPLETABUSEARCH_HH_*/
