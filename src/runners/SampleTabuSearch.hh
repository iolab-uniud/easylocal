#ifndef SAMPLETABUSEARCH_HH_
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

#include "MoveRunner.hh"
#include "../basics/EasyLocalException.hh"
#include "../helpers/StateManager.hh"
#include "../helpers/NeighborhoodExplorer.hh"
#include "helpers/TabuListManager.hh"
#include "TabuSearch.hh"

template <class Input, class State, class Move, typename CFtype = int>
class SampleTabuSearch
            : public TabuSearch<Input,State, Move>
{
public:
    void Print(std::ostream& os = std::cout) const;
    // FIXME: to implement
//    void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout)
//    throw(EasyLocalException);
    void SetSampleSize(unsigned int s)
    { sample_size = s; }
    SampleTabuSearch(const Input& in, StateManager<Input,State,CFtype>& s,
               NeighborhoodExplorer<Input,State,Move>& ne,
										 TabuListManager<State,Move,CFtype>& tlm, const std::string& name = "Anonymous Sample Tabu Search runner");
protected:
    void SelectMove();
    unsigned int sample_size;
};

/*************************************************************************
 * Implementation
 *************************************************************************/


template <class Input, class State, class Move, typename CFtype = int>
SampleTabuSearch<Input,State,Move>::SampleTabuSearch(const Input& in, StateManager<Input,State,CFtype>& sm,
        NeighborhoodExplorer<Input,State,Move>& ne,
        TabuListManager<State,Move,CFtype>& tlm,
																										 const std::string& name)
        : TabuSearch<Input,State,Move>(sm, ne, tlm, name)
{}


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
void TabuSearch<Input,State,Move>::SelectMove()
{
    unsigned int s = 1;
    register CFtype mv_cost;
    Move mv;
    bool tabu_move;
    bool all_moves_tabu = true;

    this->p_nhe->RandomMove(this->current_state, mv);
    mv_cost = this->p_nhe->DeltaCostFunction(this->current_state, mv);
    Move best_move = mv;
    CFtype best_delta = mv_cost;
    do
    {
        tabu_move = p_pm->ProhibitedMove(this->current_state, mv, mv_cost);
        if (   (mv_cost < best_delta && !tabu_move)
                || (mv_cost < best_delta && all_moves_tabu)
                || (all_moves_tabu && !tabu_move))
        {
            best_move = mv;
            best_delta = mv_cost;
        }
        if (!tabu_move)
            all_moves_tabu = false;
        this->p_nhe->RandomMove(this->current_state, mv);
        mv_cost = DeltaCostFunction(this->current_state, mv);
        s++;
    }
    while (s < sample_size);
    
    this->current_move = best_move;
    this->current_move_cost = best_delta;
}
#endif /*SAMPLETABUSEARCH_HH_*/
