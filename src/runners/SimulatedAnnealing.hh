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

#if !defined(SIMULATEDANNEALING_HH_)
#define SIMULATEDANNEALING_HH_

#include <runners/MoveRunner.hh>
#include <helpers/StateManager.hh>
#include <helpers/NeighborhoodExplorer.hh>
#include <cmath>
#include <stdexcept>

/** The Simulated annealing runner relies on a probabilistic local
    search technique whose name comes from the fact that it
    simulates the cooling of a collection of hot vibrating atoms.

    At each iteration a candidate move is generated at random, and
    it is always accepted if it is an improving move.  Instead, if
    the move is a worsening one, the new solution is accepted with
    time decreasing probability.

    @ingroup Runners
 */
template <class Input, class State, class Move, typename CFtype = int>
class SimulatedAnnealing
: public MoveRunner<Input,State,Move,CFtype>
{
public:
	SimulatedAnnealing(const Input& in,
										 StateManager<Input,State,CFtype>& e_sm,
										 NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
										 std::string name);
	SimulatedAnnealing(const Input& in,
										 StateManager<Input,State,CFtype>& e_sm,
										 NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
										 std::string name,
										 CLParser& cl);	
  SimulatedAnnealing(const Input& in,
										 StateManager<Input,State,CFtype>& e_sm,
										 NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
										 std::string name, AbstractTester<Input,State,CFtype>& t);
	SimulatedAnnealing(const Input& in,
										 StateManager<Input,State,CFtype>& e_sm,
										 NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
										 std::string name,
										 CLParser& cl, AbstractTester<Input,State,CFtype>& t);
	void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
	void Print(std::ostream& os = std::cout) const;
  void SetStartTemperature(double st)  { start_temperature = st; }
  void SetMinTemperature(double st)  { min_temperature = st; }
  void SetCoolingRate(double cr)  { cooling_rate = cr; }
  void SetNeighborsSampled(unsigned int ns)  { neighbors_sampled = ns; }
  unsigned int NeighborsSampled() const { return neighbors_sampled; }


protected:
	void GoCheck() const;
	void InitializeRun();
	bool StopCriterion();
	void UpdateIterationCounter();
	void SelectMove();
	bool AcceptableMove();
	void StoreMove();
	// parameters
	double temperature; /**< The current temperature. */
	double start_temperature;
	double min_temperature;
	double cooling_rate;
	unsigned int neighbors_sampled;
	ArgumentGroup simulated_annealing_arguments;
	ValArgument<double> arg_start_temperature, arg_min_temperature, arg_cooling_rate;
	ValArgument<unsigned int> arg_neighbors_sampled;
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
SimulatedAnnealing<Input,State,Move,CFtype>::SimulatedAnnealing(const Input& in,
																																StateManager<Input,State,CFtype>& e_sm,
																																NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
																																std::string name)
: MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name),
start_temperature(0.0), min_temperature(0.0001), cooling_rate(0.75), neighbors_sampled(10), 
simulated_annealing_arguments("sa_" + name, "sa_" + name, false), arg_start_temperature("start_temperature", "st", false),
arg_min_temperature("min_temperature", "mt", false), arg_cooling_rate("cooling_rate", "cr", true),
arg_neighbors_sampled("neighbors_sampled", "ns", true)
{
	simulated_annealing_arguments.AddArgument(arg_start_temperature);
	simulated_annealing_arguments.AddArgument(arg_min_temperature);
	simulated_annealing_arguments.AddArgument(arg_cooling_rate);
	simulated_annealing_arguments.AddArgument(arg_neighbors_sampled);
}

template <class Input, class State, class Move, typename CFtype>
SimulatedAnnealing<Input,State,Move,CFtype>::SimulatedAnnealing(const Input& in,
																																StateManager<Input,State,CFtype>& e_sm,
																																NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
																																std::string name,
																																CLParser& cl)
: MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name),
start_temperature(0.0), min_temperature(0.0001), cooling_rate(0.75), neighbors_sampled(10), 
simulated_annealing_arguments("sa_" + name, "sa_" + name, false), arg_start_temperature("start_temperature", "st", false),
arg_min_temperature("min_temperature", "mt", false), arg_cooling_rate("cooling_rate", "cr", true),
arg_neighbors_sampled("neighbors_sampled", "ns", true)
{
	simulated_annealing_arguments.AddArgument(arg_start_temperature);
	simulated_annealing_arguments.AddArgument(arg_min_temperature);
	simulated_annealing_arguments.AddArgument(arg_cooling_rate);
	simulated_annealing_arguments.AddArgument(arg_neighbors_sampled);	
	cl.AddArgument(simulated_annealing_arguments);
	cl.MatchArgument(simulated_annealing_arguments);
	if (simulated_annealing_arguments.IsSet())
	{
		if (arg_start_temperature.IsSet())
			start_temperature = arg_start_temperature.GetValue();
		if (arg_min_temperature.IsSet())
			min_temperature = arg_min_temperature.GetValue();
		cooling_rate = arg_cooling_rate.GetValue();
		neighbors_sampled = arg_neighbors_sampled.GetValue();
	}
}

template <class Input, class State, class Move, typename CFtype>
SimulatedAnnealing<Input,State,Move,CFtype>::SimulatedAnnealing(const Input& in,
																																StateManager<Input,State,CFtype>& e_sm,
																																NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
																																std::string name, AbstractTester<Input,State,CFtype>& t)
: MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name, t),
start_temperature(0.0), min_temperature(0.0001), cooling_rate(0.75), neighbors_sampled(10), 
simulated_annealing_arguments("sa_" + name, "sa_" + name, false), arg_start_temperature("start_temperature", "st", false),
arg_min_temperature("min_temperature", "mt", false), arg_cooling_rate("cooling_rate", "cr", true),
arg_neighbors_sampled("neighbors_sampled", "ns", true)
{
	simulated_annealing_arguments.AddArgument(arg_start_temperature);
	simulated_annealing_arguments.AddArgument(arg_min_temperature);
	simulated_annealing_arguments.AddArgument(arg_cooling_rate);
	simulated_annealing_arguments.AddArgument(arg_neighbors_sampled);
}

template <class Input, class State, class Move, typename CFtype>
SimulatedAnnealing<Input,State,Move,CFtype>::SimulatedAnnealing(const Input& in,
																																StateManager<Input,State,CFtype>& e_sm,
																																NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
																																std::string name,
																																CLParser& cl, AbstractTester<Input,State,CFtype>& t)
: MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name, t),
start_temperature(0.0), min_temperature(0.0001), cooling_rate(0.75), neighbors_sampled(10), 
simulated_annealing_arguments("sa_" + name, "sa_" + name, false), arg_start_temperature("start_temperature", "st", false),
arg_min_temperature("min_temperature", "mt", false), arg_cooling_rate("cooling_rate", "cr", true),
arg_neighbors_sampled("neighbors_sampled", "ns", true)
{
	simulated_annealing_arguments.AddArgument(arg_start_temperature);
	simulated_annealing_arguments.AddArgument(arg_min_temperature);
	simulated_annealing_arguments.AddArgument(arg_cooling_rate);
	simulated_annealing_arguments.AddArgument(arg_neighbors_sampled);	
	cl.AddArgument(simulated_annealing_arguments);
	cl.MatchArgument(simulated_annealing_arguments);
	if (simulated_annealing_arguments.IsSet())
	{
		if (arg_start_temperature.IsSet())
			start_temperature = arg_start_temperature.GetValue();
		if (arg_min_temperature.IsSet())
			min_temperature = arg_min_temperature.GetValue();
		cooling_rate = arg_cooling_rate.GetValue();
		neighbors_sampled = arg_neighbors_sampled.GetValue();
	}
}



template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealing<Input,State,Move,CFtype>::Print(std::ostream& os) const
{
    os  << "Simulated Annealing Runner: " << std::endl;
    os  << "  Max iterations: " << this->max_iteration << std::endl;
    os  << "  Start temperature: " << start_temperature << std::endl;
    os  << "  Min temperature: " << min_temperature << std::endl;
    os  << "  Cooling rate: " << cooling_rate << std::endl;
    os  << "  Neighbors sampled: " << neighbors_sampled << std::endl;
}

template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealing<Input,State,Move,CFtype>::GoCheck() const

{
	if (start_temperature < 0)
		throw std::logic_error("negative start_temparature for object " + this->name);
	if (cooling_rate <= 0)
		throw std::logic_error("negative cooling_rate for object " + this->name)
		;
	if (neighbors_sampled == 0)
		throw std::logic_error("neighbors_sampled is zero for object " + this->name);
}

template <typename CFtype>
CFtype max(const std::vector<CFtype>& values) 
{
  CFtype max_val = values[0];
  for (unsigned int i = 1; i < values.size(); i++)
    if (values[i] > max_val)
      max_val = values[i];
  
  return max_val;
}

/**
   Initializes the run by invoking the companion superclass method, and
   setting the temperature to the start value.
*/
template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealing<Input,State,Move,CFtype>::InitializeRun()
{
  const unsigned int samples = 100;
	MoveRunner<Input,State,Move,CFtype>::InitializeRun();
	if (start_temperature > 0.0)
		temperature = start_temperature;
	else
	{
		// Compute a start temperature by sampling the search space and computing the variance
		// according to [van Laarhoven and Aarts, 1987] (allow an acceptance ratio of approximately 80%)
    //State sampled_state(this->in);
		std::vector<CFtype> cost_values(samples);
		//		double mean = 0.0, variance = 0.0;
		for (unsigned int i = 0; i < samples; i++)
		{
			//this->sm.RandomState(sampled_state);
      Move mv;
      this->ne.RandomMove(this->current_state, mv);
			cost_values[i] = this->ne.DeltaCostFunction(this->current_state, mv);
			//mean += cost_values[i];
		}
		/* mean /= samples;
		for (unsigned int i = 0; i < samples; i++)
			variance += (cost_values[i] - mean) * (cost_values[i] - mean) / samples;
		temperature = variance; */
    temperature = max(cost_values);
    //		std::cerr << "Temperature: " << temperature << std::endl;
	}
}

/**
   A move is randomly picked.
*/
template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealing<Input,State,Move,CFtype>::SelectMove()
{
    this->ne.RandomMove(this->current_state, this->current_move);
    this->ComputeMoveCost();
}

/**
   A move is randomly picked.
*/
template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealing<Input,State,Move,CFtype>::StoreMove()
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
void SimulatedAnnealing<Input,State,Move,CFtype>::ReadParameters(std::istream& is, std::ostream& os)

{
    os << "SIMULATED ANNEALING -- INPUT PARAMETERS" << std::endl;
    os << "  Start temperature: ";
    is >> start_temperature;
    os << "  Min temperature: ";
    is >> min_temperature;
    os << "  Cooling rate: ";
    is >> cooling_rate;
    os << "  Neighbors sampled at each temperature: ";
    is >> neighbors_sampled;
}

/**
   The search stops when a low temperature has reached.
 */
template <class Input, class State, class Move, typename CFtype>
bool SimulatedAnnealing<Input,State,Move,CFtype>::StopCriterion()
{ return temperature <= min_temperature; }

/**
   At regular steps, the temperature is decreased 
   multiplying it by a cooling rate.
*/
template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealing<Input,State,Move,CFtype>::UpdateIterationCounter()
{
	MoveRunner<Input,State,Move,CFtype>::UpdateIterationCounter();
	if (this->number_of_iterations % neighbors_sampled == 0)
		temperature *= cooling_rate;
}

/** A move is surely accepted if it improves the cost function
    or with exponentially decreasing probability if it is 
    a worsening one.
*/
template <class Input, class State, class Move, typename CFtype>
bool SimulatedAnnealing<Input,State,Move,CFtype>::AcceptableMove()
{ 
  return LessOrEqualThan(this->current_move_cost,(CFtype)0)
    || (Random::Double_Unit_Uniform() < exp(-this->current_move_cost/temperature)); 
}

#endif /*SIMULATEDANNEALING_HH_*/
