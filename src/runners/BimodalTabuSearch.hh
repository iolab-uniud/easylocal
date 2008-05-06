#ifndef BIMODALTABUSEARCH_HH_
#define BIMODALTABUSEARCH_HH_

#include <runners/BimodalMoveRunner.hh>
#include <stdexcept>

template <class Input, class State, class Move1, class Move2, typename CFtype = int>
class BimodalTabuSearch
: public BimodalMoveRunner<Input,State, Move1, Move2,CFtype>
{
public:
  void Print(std::ostream& os = std::cout) const;
  void ReadParameters(std::istream& is = std::cin, std::ostream& os = std::cout);
  virtual void SetMaxIdleIteration(unsigned long m) 
  { max_idle_iteration = m; }
  void SetTabuTenure1(unsigned int min, unsigned int max) 
  { pm1.SetLength(min, max); }
  void SetTabuTenure2(unsigned int min, unsigned int max) 
  { pm2.SetLength(min, max); }
  BimodalTabuSearch(const Input& in, 
		    StateManager<Input,State,CFtype>& sm,
		    NeighborhoodExplorer<Input,State,Move1,CFtype>& ne1,
		    NeighborhoodExplorer<Input,State,Move2,CFtype>& ne2,
		    TabuListManager<State, Move1,CFtype>& tlm1,
		    TabuListManager<State, Move2,CFtype>& tlm2,
		    std::string name = "Anonymous Bimodal Tabu Search runner");
protected:
  void GoCheck() const;
  void InitializeRun();
  bool StopCriterion();
  void SelectMove();
  virtual void SelectMove1();
  virtual void SelectMove2();
  bool AcceptableMove();
  void StoreMove();
  void TerminateRun();
  TabuListManager<State, Move1,CFtype>& pm1; /**< A pointer to a tabu list manger. */
  TabuListManager<State, Move2,CFtype>& pm2; /**< A pointer to a tabu list manger. */
  // parameters
  unsigned long max_idle_iteration;
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
template <class Input, class State, class Move1, class Move2, typename CFtype>
BimodalTabuSearch<Input,State,Move1,Move2,CFtype>::BimodalTabuSearch(const Input& in,
							      StateManager<Input,State,CFtype>& sm,
							      NeighborhoodExplorer<Input,State,Move1,CFtype>& ne1,
							      NeighborhoodExplorer<Input,State,Move2,CFtype>& ne2,
							      TabuListManager<State, Move1,CFtype>& tlm1,
							      TabuListManager<State, Move2,CFtype>& tlm2,
							      std::string name)
  : BimodalMoveRunner<Input,State,Move1,Move2,CFtype>(in, sm, ne1, ne2, name), pm1(tlm1), pm2(tlm2), max_idle_iteration(0)
{}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalTabuSearch<Input,State,Move1,Move2,CFtype>::Print(std::ostream& os) const
{
  os  << "Bimodal Tabu Search Runner: " << this->name << std::endl;
  
  os  << "Max iterations: " << this->max_iteration << std::endl;
  os  << "Max idle iteration: " << this->max_idle_iteration << std::endl;
  pm1.Print(os);
  pm2.Print(os);
  
}

/**
   Initializes the run by invoking the companion superclass method, and
   cleans the tabu list.
*/
template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalTabuSearch<Input,State,Move1,Move2,CFtype>::InitializeRun()
{
  BimodalMoveRunner<Input,State,Move1,Move2,CFtype>::InitializeRun();
  pm1.Clean();
  pm2.Clean();
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalTabuSearch<Input,State,Move1,Move2,CFtype>::GoCheck() const
{
  if (this->max_idle_iteration == 0)
    throw std::logic_error("max_idle_iteration is zero for object " + this->name);
}


/**
Selects always the best move that is non prohibited by the tabu list 
 mechanism.
 */
template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalTabuSearch<Input,State,Move1,Move2,CFtype>::SelectMove()
{
  // FIXME: to review
  SelectMove1();
  SelectMove2();
  if (LessThan(this->current_move_cost1, this->current_move_cost2))
    this->current_move_type = MOVE_1;
  else if (LessThan(this->current_move_cost2, this->current_move_cost1))
    this->current_move_type = MOVE_2;
  else
    this->current_move_type = Random::Int(0,1) == 0 ? MOVE_1 : MOVE_2;
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalTabuSearch<Input,State,Move1,Move2,CFtype>::SelectMove1() 
{
	Move1 mv;
	register CFtype mv_cost;
	register bool all_moves_tabu = true;
	
	this->ne1.FirstMove(this->current_state, mv);
	mv_cost = this->ne1.DeltaCostFunction(this->current_state, mv);
	Move1 best_move = mv;
	CFtype best_delta = mv_cost;
	do  // look for the best non prohibited move
	{   // (if all moves are prohibited, then get the best)
			// For efficency, ProhibitedMove is invoked only when strictly necessary
		if (mv_cost < best_delta)
		{
			if (!pm1.ProhibitedMove(this->current_state, mv, mv_cost))
			{
				best_move = mv;
				best_delta = mv_cost;
				all_moves_tabu = false;
			}
			if (all_moves_tabu)
			{
				best_move = mv;
				best_delta = mv_cost;
			}
		}
		else if (all_moves_tabu && !pm1.ProhibitedMove(this->current_state, mv, mv_cost))
		{ // even though it is not an improving move,
			// this move is the actual best since it's the first non-tabu
			best_move = mv;
			best_delta = mv_cost;
			all_moves_tabu = false;
		}
		this->ne1.NextMove(this->current_state, mv);
		mv_cost = this->ne1.DeltaCostFunction(this->current_state, mv);
	}
	while (!this->ne1.LastMoveDone(this->current_state, mv));
	
	this->current_move1 = best_move;
	this->current_move_cost1 = best_delta;
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalTabuSearch<Input,State,Move1,Move2,CFtype>::SelectMove2() 
{
	Move2 mv;
	register CFtype mv_cost;
	register bool all_moves_tabu = true;
	
	this->ne2.FirstMove(this->current_state, mv);
	mv_cost = this->ne2.DeltaCostFunction(this->current_state, mv);
	Move2 best_move = mv;
	CFtype best_delta = mv_cost;
	do  // look for the best non prohibited move
	{   // (if all moves are prohibited, then get the best)
			// For efficency, ProhibitedMove is invoked only when strictly necessary
		if (mv_cost < best_delta)
		{
			if (!pm2.ProhibitedMove(this->current_state, mv, mv_cost))
			{
				best_move = mv;
				best_delta = mv_cost;
				all_moves_tabu = false;
			}
			if (all_moves_tabu)
			{
				best_move = mv;
				best_delta = mv_cost;
			}
		}
		else if (all_moves_tabu && !pm2.ProhibitedMove(this->current_state, mv, mv_cost))
		{ // even though it is not an improving move,
			// this move is the actual best since it's the first non-tabu
			best_move = mv;
			best_delta = mv_cost;
			all_moves_tabu = false;
		}
		this->ne2.NextMove(this->current_state, mv);
		mv_cost = this->ne2.DeltaCostFunction(this->current_state, mv);
	}
	while (!this->ne2.LastMoveDone(this->current_state, mv));
	
	this->current_move2 = best_move;
	this->current_move_cost2 = best_delta;
}

/**
The stop criterion is based on the number of iterations elapsed from
 the last strict improvement of the best state cost.
 */
template <class Input, class State, class Move1, class Move2, typename CFtype>
bool BimodalTabuSearch<Input,State,Move1,Move2,CFtype>::StopCriterion()
{ return this->number_of_iterations - this->iteration_of_best >= this->max_idle_iteration; }

/**
In tabu search the selected move is always accepted.
 That is, the acceptability test is replaced by the 
 prohibition mechanism which is managed inside the selection.
 */
template <class Input, class State, class Move1, class Move2, typename CFtype>
bool BimodalTabuSearch<Input,State,Move1,Move2,CFtype>::AcceptableMove()
{ return true; }


/**
Stores the move by inserting it in the tabu list, if the state obtained
 is better than the one found so far also the best state is updated.
 */
template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalTabuSearch<Input,State,Move1,Move2,CFtype>::StoreMove()
{
	if (this->current_move_type == MOVE_1)
	{
		this->pm1.InsertMove(this->current_state, this->current_move1, this->current_move_cost1, this->current_state_cost, this->best_state_cost);
		this->pm2.UpdateIteration();
	}
	else
	{
		this->pm2.InsertMove(this->current_state, this->current_move2, this->current_move_cost2, this->current_state_cost, this->best_state_cost);
		this->pm1.UpdateIteration();
	}
	if (LessThan(this->current_state_cost,this->best_state_cost))
	{
		this->iteration_of_best = this->number_of_iterations;
		this->best_state = this->current_state;
		this->best_state_cost = this->current_state_cost;
	}
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalTabuSearch<Input,State,Move1,Move2,CFtype>::TerminateRun()
{
	BimodalMoveRunner<Input,State,Move1,Move2,CFtype>::TerminateRun();
}

template <class Input, class State, class Move1, class Move2, typename CFtype>
void BimodalTabuSearch<Input,State,Move1,Move2,CFtype>::ReadParameters(std::istream& is, std::ostream& os)
{
	os << "BIMODAL TABU SEARCH -- INPUT PARAMETERS" << std::endl;
	pm1.ReadParameters(is,os);
	pm2.ReadParameters(is,os);
	os << "  Number of idle iterations: ";
	is >> this->max_idle_iteration;
// 	os << "  Timeout: ";
// 	is >> this->timeout;
}
#endif /*BIMODALTABUSEARCH_HH_*/
