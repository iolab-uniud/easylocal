#ifndef TRIMODALTABUSEARCHWITHSHIFTINGPENALTY_HH_
#define TRIMODALTABUSEARCHWITHSHIFTINGPENALTY_HH_

template <class Input, class State, class Move1, class Move2, class Move3>
class TrimodalTabuSearchWithShiftingPenalty
            : public TrimodalTabuSearch<Input,State,Move1,Move2,Move3>
{
public:
    void Print(std::ostream& os = std::cout) const;
    void ReadParameters(std::istream& is = std::cin,
                        std::ostream& os = std::cout)
    throw(EasyLocalException);
protected:
    TrimodalTabuSearchWithShiftingPenalty(StateManager<Input,State,CFtype>* s,
                                          NeighborhoodExplorer<Input,State,Move1>* ne1,
                                          NeighborhoodExplorer<Input,State,Move2>* ne2,
                                          NeighborhoodExplorer<Input,State,Move3>* ne3,
                                          TabuListManager<State, Move1>* tlm1,
                                          TabuListManager<State, Move2>* tlm2,
                                          TabuListManager<State, Move3>* tlm3,
                                          Input* in = NULL);
    ~TrimodalTabuSearchWithShiftingPenalty();
    void InitializeRun();
    void SelectMove();
    void MakeMove();
    void StoreMove();
    NeighborhoodExplorerWithShiftingPenalty<Input,State,Move1>* p_nhewsp1;
    NeighborhoodExplorerWithShiftingPenalty<Input,State,Move2>* p_nhewsp2;
    NeighborhoodExplorerWithShiftingPenalty<Input,State,Move3>* p_nhewsp3;
    // parameters
    double weight_region;
};

/*************************************************************************
 * Implementation
 *************************************************************************/
 
template <class Input, class State, class Move1, class Move2, class Move3>
TrimodalTabuSearchWithShiftingPenalty<Input,State,Move1,Move2,Move3>::TrimodalTabuSearchWithShiftingPenalty(StateManager<Input,State,CFtype>* s,
        NeighborhoodExplorer<Input,State,Move1>* ne1,
        NeighborhoodExplorer<Input,State,Move2>* ne2,
        NeighborhoodExplorer<Input,State,Move3>* ne3,
        TabuListManager<State, Move1>* tlm1,
        TabuListManager<State, Move2>* tlm2,
        TabuListManager<State, Move3>* tlm3,
        Input* in)
        :  TrimodalTabuSearch<Input,State,Move1,Move2,Move3>(s,ne1,ne2,ne3,tlm1,tlm2,tlm3,in), weight_region(0.9)
{
    p_nhewsp1 = new NeighborhoodExplorerWithShiftingPenalty<Input,State,Move1>(this->p_nhe1);
    p_nhewsp2 = new NeighborhoodExplorerWithShiftingPenalty<Input,State,Move2>(this->p_nhe2);
    p_nhewsp3 = new NeighborhoodExplorerWithShiftingPenalty<Input,State,Move3>(this->p_nhe3);
    this->p_nhep1->ChangeNHEComponent(p_nhewsp1);
    this->p_nhep2->ChangeNHEComponent(p_nhewsp2);
    this->p_nhep3->ChangeNHEComponent(p_nhewsp3);
}

template <class Input, class State, class Move1, class Move2, class Move3>
TrimodalTabuSearchWithShiftingPenalty<Input,State,Move1,Move2,Move3>::~TrimodalTabuSearchWithShiftingPenalty()
{
    delete p_nhewsp1;
    delete p_nhewsp2;
    delete p_nhewsp3;
}

template <class Input, class State, class Move1, class Move2, class Move3>
void TrimodalTabuSearchWithShiftingPenalty<Input,State,Move1,Move2,Move3>::Print(std::ostream& os) const
{
    os  << "Trimodal Tabu Search with Shifting Penalty Runner: " << this->GetName() << std::endl;
    os  << "  Max iterations: " << this->max_iteration << std::endl;
    os  << "  Max idle iteration: " << this->max_idle_iteration << std::endl;
    this->p_pm1->Print(os);
    this->p_pm2->Print(os);
    this->p_pm3->Print(os);
    os  << "  Weight region: " << weight_region << std::endl;
}


template <class Input, class State, class Move1, class Move2, class Move3>
void TrimodalTabuSearchWithShiftingPenalty<Input,State,Move1,Move2,Move3>::InitializeRun()
{
    TrimodalTabuSearch<Input,State,Move1,Move2,Move3>::InitializeRun();
    p_nhewsp1->ResetWeights(this->current_state);
    p_nhewsp2->ResetWeights(this->current_state);
    p_nhewsp3->ResetWeights(this->current_state);
}

template <class Input, class State, class Move1, class Move2, class Move3>
void TrimodalTabuSearchWithShiftingPenalty<Input,State,Move1,Move2,Move3>::SelectMove()
{
    //    if (this->number_of_iterations - this->iteration_of_best <= weight_region * this->max_idle_iteration) {
    this->current_move_cost1 = this->p_nhep1->BestMove(this->current_state, this->current_move1);
    this->current_move_cost2 = this->p_nhep2->BestMove(this->current_state, this->current_move2);
    this->current_move_cost3 = this->p_nhep3->BestMove(this->current_state, this->current_move3);
    //       cerr << '(' << this->current_move_cost1 << ' ' << this->current_move_cost2 << ") ";
    if (this->current_move_cost1 < this->current_move_cost2)
        if (this->current_move_cost1 < this->current_move_cost3)
            this->current_move_type = MOVE_1;
        else if (this->current_move_cost1 > this->current_move_cost3)
            this->current_move_type = MOVE_3;
        else
            this->current_move_type = Random::Int(0,1) == 0 ? MOVE_1 : MOVE_3;
    else if (this->current_move_cost1 > this->current_move_cost2)
        if (this->current_move_cost2 < this->current_move_cost3)
            this->current_move_type = MOVE_2;
        else if (this->current_move_cost2 > this->current_move_cost3)
            this->current_move_type = MOVE_3;
        else
            this->current_move_type = Random::Int(0,1) == 0 ? MOVE_2 : MOVE_3;
    else
        this->current_move_type = Random::Int(0,1) == 0 ? MOVE_1 : MOVE_2;
    //    }
    //    else
    // TrimodalTabuSearch<Input,State,Move1,Move2,Move3>::SelectMove();
}

template <class Input, class State, class Move1, class Move2, class Move3>
void TrimodalTabuSearchWithShiftingPenalty<Input,State,Move1,Move2,Move3>::MakeMove()
{
    //    if (this->number_of_iterations - this->iteration_of_best <= weight_region * this->max_idle_iteration)
    switch (this->current_move_type)
    {
    case MOVE_1:
        this->current_move_cost1 = this->p_nhe1->DeltaCostFunction(this->current_state, this->current_move1);
        this->p_nhepwsp1->MakeMove(this->current_state, this->current_move1);
        break;
    case MOVE_2:
        this->current_move_cost2 = this->p_nhe2->DeltaCostFunction(this->current_state, this->current_move2);
        this->p_nhepwsp2->MakeMove(this->current_state, this->current_move2);
        break;
    case MOVE_3:
        this->current_move_cost3 = this->p_nhe3->DeltaCostFunction(this->current_state, this->current_move3);
        this->p_nhepwsp3->MakeMove(this->current_state, this->current_move3);
        break;
    }
    //    else
    //TrimodalTabuSearch<Input,State,Move1,Move2,Move3>::MakeMove();
}

template <class Input, class State, class Move1, class Move2, class Move3>
void TrimodalTabuSearchWithShiftingPenalty<Input,State,Move1,Move2,Move3>::StoreMove()
{
  if (LessThan(this->current_state_cost,this->best_state_cost))
    {
        p_nhewsp1->ResetWeights(this->current_state);
        p_nhewsp2->ResetWeights(this->current_state);
        p_nhewsp3->ResetWeights(this->current_state);
    }
    TrimodalTabuSearch<Input,State,Move1,Move2,Move3>::StoreMove();
}


template <class Input, class State, class Move1, class Move2, class Move3>
void TrimodalTabuSearchWithShiftingPenalty<Input,State,Move1,Move2,Move3>::ReadParameters(std::istream& is, std::ostream& os)
throw(EasyLocalException)
{
    os << "TRIMODAL TABU SEARCH WITH SHIFTING PENALTY -- INPUT PARAMETERS" << std::endl;
    TrimodalTabuSearch<Input,State,Move1,Move2,Move3>::ReadParameters(is,os);
    os << "  Weight region (fraction of idle iterations): ";
    is >> weight_region;
}

#endif /*TRIMODALTABUSEARCHWITHSHIFTINGPENALTY_HH_*/
