#ifndef THRESHOLDACCEPTANCE_HH_
#define THRESHOLDACCEPTANCE_HH_

#include <MoveRunner.hh>
#include <helpers/StateManager.hh>
#include <helpers/NeighborhoodExplorer.hh>
#include <basics/std::logic_exception.hh>
#include <cmath>

/** @ingroup Runners
 */
template <class Input, class State, class Move, typename CFtype = int>
class ThresholdAcceptance
            : public MoveRunner<Input,State,Move>
{
public:
    ThresholdAcceptance(const Input& in,
                       StateManager<Input,State,CFtype>& e_sm,
                       NeighborhoodExplorer<Input,State,Move>& e_ne,
                        std::string name = "Anonymous Threshold Acceptance runner");

    void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout)
   ;
    void Print(std::ostream& os = std::cout) const;
protected:
    void GoCheck() const;
    void InitializeRun();
    void TerminateRun();
    bool StopCriterion();
    void UpdateIterationCounter();
    void SelectMove();
    bool AcceptableMove();
    void StoreMove();
    // parameters
    CFtype threshold; /**< The current threshold. */
    CFtype start_threshold;
    CFtype min_threshold;
    double threshold_rate;
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
template <class Input, class State, class Move, typename CFtype = int>
ThresholdAcceptance<Input,State,Move>::ThresholdAcceptance(const Input& in,
        StateManager<Input,State,CFtype>& e_sm,
        NeighborhoodExplorer<Input,State,Move>& e_ne,
        std::string name)
        : MoveRunner<Input,State,Move>(in, e_sm, e_ne, name),
        start_threshold(2.0), min_threshold(0.0001),
        threshold_rate(0.75), neighbors_sampled(1)
{}

template <class Input, class State, class Move, typename CFtype = int>
void ThresholdAcceptance<Input,State,Move>::Print(std::ostream& os) const
{
    os  << "Simulated Annealing Runner: " << std::endl;
    os  << "  Max iterations: " << this->max_iteration << std::endl;
    os  << "  Start threshold: " << start_threshold << std::endl;
    os  << "  Min threshold: " << min_threshold << std::endl;
    os  << "  Threshold rate: " << threshold_rate << std::endl;
    os  << "  Neighbors sampled: " << neighbors_sampled << std::endl;
}


/**
   Initializes the run by invoking the companion superclass method, and
   setting the threshold to the start value.
*/
template <class Input, class State, class Move, typename CFtype = int>
void ThresholdAcceptance<Input,State,Move>::InitializeRun()
{
    MoveRunner<Input,State,Move>::InitializeRun();
    threshold = start_threshold;
}

template <class Input, class State, class Move, typename CFtype = int>
void ThresholdAcceptance<Input,State,Move>::GoCheck() const

{
    if (start_threshold <= 0)
        throw std::logic_exception("negative start_temparature for object " + this->GetName());
    if (threshold_rate <= 0)
        throw std::logic_exception("negative threshold_rate for object " + this->GetName());
    if (neighbors_sampled == 0)
        throw std::logic_exception("neighbors_sampled is zero for object " + this->GetName());
}

/**
   Stores the current state as best state (it is obviously a local minimum).
*/
template <class Input, class State, class Move, typename CFtype = int>
void ThresholdAcceptance<Input,State,Move>::TerminateRun()
{
    MoveRunner<Input,State,Move>::TerminateRun();
    this->best_state = this->current_state;
    this->best_state_cost = this->current_state_cost;
}

/**
   A move is randomly picked.
*/
template <class Input, class State, class Move, typename CFtype = int>
void ThresholdAcceptance<Input,State,Move>::SelectMove()
{
    this->ne.RandomMove(this->current_state, this->current_move);
    this->ComputeMoveCost();
}

/**
   A move is randomly picked.
*/
template <class Input, class State, class Move, typename CFtype = int>
void ThresholdAcceptance<Input,State,Move>::StoreMove()
{
  if (LessThan(this->current_move_cost,0))
    {
        this->best_state_cost = this->current_state_cost;
    }
}

template <class Input, class State, class Move, typename CFtype = int>
void ThresholdAcceptance<Input,State,Move>::ReadParameters(std::istream& is, std::ostream& os)

{
    os << "THRESHOLD ACCEPTANCE -- INPUT PARAMETERS" << std::endl;
    os << "  Start threshold: ";
    is >> start_threshold;
    os << "  Cooling rate: ";
    is >> threshold_rate;
    os << "  Neighbors sampled at each threshold: ";
    is >> neighbors_sampled;
}

/**
   The search stops when a low threshold has reached.
 */
template <class Input, class State, class Move, typename CFtype = int>
bool ThresholdAcceptance<Input,State,Move>::StopCriterion()
{ return threshold <= min_threshold; }

/**
   At regular steps, the threshold is decreased 
   multiplying it by a threshold rate.
*/
template <class Input, class State, class Move, typename CFtype = int>
void ThresholdAcceptance<Input,State,Move>::UpdateIterationCounter()
{
    MoveRunner<Input,State,Move>::UpdateIterationCounter();
    if (this->number_of_iterations % neighbors_sampled == 0)
        threshold *= threshold_rate;
}

/** A move is surely accepted if it improves the cost function
    or with exponentially decreasing probability if it is 
    a worsening one.
*/
template <class Input, class State, class Move, typename CFtype = int>
bool ThresholdAcceptance<Input,State,Move>::AcceptableMove()
{ return (this->current_move_cost <= threshold); }

#endif /*SIMULATEDANNEALING_HH_*/
