#ifndef SIMULATEDANNEALING_HH_
#define SIMULATEDANNEALING_HH_

#include "MoveRunner.hh"
#include "../helpers/StateManager.hh"
#include "../helpers/NeighborhoodExplorer.hh"
#include "../basics/EasyLocalException.hh"
#include <cmath>

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
                       const std::string& name = "Anonymous Simulated Annealing runner");

    void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout)
    throw(EasyLocalException);
    void Print(std::ostream& os = std::cout) const;
protected:
    void GoCheck() const throw(EasyLocalException);
    void InitializeRun();
    void TerminateRun();
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
        const std::string& name)
        : MoveRunner<Input,State,Move,CFtype>(in, e_sm, e_ne, name),
        start_temperature(2.0), min_temperature(0.0001),
        cooling_rate(0.75), neighbors_sampled(1)
{}

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


/**
   Initializes the run by invoking the companion superclass method, and
   setting the temperature to the start value.
*/
template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealing<Input,State,Move,CFtype>::InitializeRun()
{
    MoveRunner<Input,State,Move,CFtype>::InitializeRun();
    temperature = start_temperature;
}

template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealing<Input,State,Move,CFtype>::GoCheck() const
throw(EasyLocalException)
{
    if (start_temperature <= 0)
        throw EasyLocalException("negative start_temparature for object " + this->GetName());
    if (cooling_rate <= 0)
        throw EasyLocalException("negative cooling_rate for object " + this->GetName());
    if (neighbors_sampled == 0)
        throw EasyLocalException("neighbors_sampled is zero for object " + this->GetName());
}

/**
   Stores the current state as best state (it is obviously a local minimum).
*/
template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealing<Input,State,Move,CFtype>::TerminateRun()
{
    MoveRunner<Input,State,Move,CFtype>::TerminateRun();
    this->best_state = this->current_state;
    this->best_state_cost = this->current_state_cost;
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
  if (LessThan(this->current_move_cost,0))
    {
        this->best_state_cost = this->current_state_cost;
    }
}

template <class Input, class State, class Move, typename CFtype>
void SimulatedAnnealing<Input,State,Move,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
throw(EasyLocalException)
{
    os << "SIMULATED ANNEALING -- INPUT PARAMETERS" << std::endl;
    os << "  Start temperature: ";
    is >> start_temperature;
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
{ return (this->current_move_cost <= 0)
  || (Random::Double_Unit_Uniform() < exp(-this->current_move_cost/temperature)); }

#endif /*SIMULATEDANNEALING_HH_*/
