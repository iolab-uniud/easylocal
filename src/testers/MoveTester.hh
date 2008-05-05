#ifndef MOVETESTER_HH_
#define MOVETESTER_HH_

#include "ComponentTester.hh"
#include <helpers/OutputManager.hh>
#include <helpers/NeighborhoodExplorer.hh>

/** A Move Tester allows to test the behavior of a given
    neighborhood explorer.
    @ingroup Testers
*/
template <class Input, class Output, class State, class Move, typename CFtype = int>
class MoveTester
  : public ComponentTester<Input,Output,State,CFtype>
{
public:
  MoveTester(const Input& in,
	     StateManager<Input,State,CFtype>& e_sm,
	     OutputManager<Input,Output,State,CFtype>& e_om,
	     NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
	     std::string name);
  void PrintNeighborhoodStatistics(const State& st, std::ostream& os = std::cout) const;
  void PrintAllNeighbors(const State& st, std::ostream& os = std::cout) const;
  void CheckNeighborhoodCosts(const State& st, std::ostream& os = std::cout) const;
  void CheckMoveCosts(const State& st,  const Move& mv, 
		      std::ostream& os = std::cout) const;
  void CheckMoveIndependence(const State& st, std::ostream& os = std::cout) const;
protected:
  void ShowMenu();
  bool ExecuteChoice(State& st);
  NeighborhoodExplorer<Input,State,Move,CFtype>& ne; /**< A reference to the
							attached neighborhood
							explorer. */
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
   Constructs a move tester by providing it links to
   a state manager, an output manager, a neighborhood explorer, a name and 
   an input object.

   @param sm a pointer to a compatible state manager
   @param om a pointer to a compatible output manager
   @param ne a pointer to a compatible neighborhood explorer
   @param nm the name of the move tester
   @param in a pointer to an input object
*/
template <class Input, class Output, class State, class Move, typename CFtype>
MoveTester<Input,Output,State,Move,CFtype>::MoveTester(const Input& in,
						       StateManager<Input,State,CFtype>& e_sm,
						       OutputManager<Input,Output,State,CFtype>& e_om,
						       NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
						       std::string name)
  : ComponentTester<Input,Output,State,CFtype>(in, e_sm, e_om, name), ne(e_ne)
{}


/**
   Outputs the menu options.
*/
template <class Input, class Output, class State, class Move, typename CFtype>
void MoveTester<Input,Output,State,Move,CFtype>::ShowMenu()
{
  std::cout << "Move Menu: " << std::endl
	    << "     (1)  Perform Best Move" << std::endl
	    << "     (2)  Perform Random Move" << std::endl
	    << "     (3)  Perform Input Move" << std::endl
	    << "     (4)  Print All Neighbors" << std::endl
	    << "     (5)  Print Neighborhood Statistics" << std::endl
	    << "     (6)  Check Input Move Cost" << std::endl 
	    << "     (7)  Check Neighborhood Costs" << std::endl 
	    << "     (8)  Check Move Indenpendence" << std::endl 
	    << "     (0)  Return to Main Menu" << std::endl
	    << " Your choice: ";
  std::cin >> this->choice;
}

/**
   Execute the menu choice on the given state.

   @param st the current state
*/
template <class Input, class Output, class State, class Move, typename CFtype>
bool MoveTester<Input,Output,State,Move,CFtype>::ExecuteChoice(State& st)
{
  Move mv;
  switch(this->choice)
    {
    case 1:
      ne.BestMove(st,mv);
      break;
    case 2:
      ne.RandomMove(st,mv);
      break;
    case 3:
      std::cout << "Input move : ";
      std::cin >> mv;
      break;
    case 4:
      PrintAllNeighbors(st);
      break;
    case 5:
      PrintNeighborhoodStatistics(st);
      break;
    case 6:
      std::cout << "Input move : ";
      std::cin >> mv;
      CheckMoveCosts(st,mv);
      break;
    case 7:
      CheckNeighborhoodCosts(st);
      break;
    case 8:
      CheckMoveIndependence(st);
      break;
    default:
      std::cout << "Invalid choice" << std::endl;
    }
  if (this->choice == 1 || this->choice == 2 || this->choice == 3)
    {
      std::cout << "Move : " << mv << std::endl;
      if (ne.FeasibleMove(st,mv))
	{
	  ne.MakeMove(st,mv);
	  return true;
	}
      else
	std::cout << "Infeasible move!" << std::endl;
    }
  
  return false;
}
 
template <class Input, class Output, class State, class Move, typename CFtype>
void MoveTester<Input,Output,State,Move,CFtype>::CheckMoveCosts(const State& st, const Move& mv, 
								std::ostream& os) const
{
  State st1 = st;
  os << "Move : " << mv << std::endl;
  os << "Start state:" << std::endl;
  this->sm.PrintStateCost(st, os);
  ne.PrintMoveCost(st, mv, os);
  ne.MakeMove(st1, mv);
  os << "Final state: " << std::endl;  
  this->sm.PrintStateCost(st1, os);
  os << "Error : " << this->sm.CostFunction(st1) - ne.DeltaCostFunction(st, mv) - this->sm.CostFunction(st) << std::endl;
}

template <class Input, class Output, class State, class Move, typename CFtype>
void MoveTester<Input,Output,State,Move,CFtype>::CheckNeighborhoodCosts(const State& st, std::ostream& os) const
{
  Move mv;
  unsigned move_count = 0;
  CFtype error;
  State st1 = st;
  bool error_found = false;
  ne.FirstMove(st, mv);
  do
    {
      move_count++;
      ne.MakeMove(st1, mv);
      error = this->sm.CostFunction(st1) - ne.DeltaCostFunction(st, mv) - this->sm.CostFunction(st);
      if (!IsZero(error))
	{
	  std::cout << std::endl << "Error: Move n. " << move_count << ", Info" << std::endl;
	  CheckMoveCosts(st,mv);
	  error_found = true;
	  std::cout << "Press enter to continue " << std::endl;
	  std::cin.get();
	}          
      if (move_count % 100 == 0) std::cerr << '.'; // show that it is alive
      ne.NextMove(st, mv);
      st1 = st;
    }
  while(!ne.LastMoveDone(st, mv));
  
  if (!error_found)
    std::cout << std::endl << "No error found (for " << move_count << " moves)!" << std::endl;
}

/**
   Outputs some statistics about the neighborhood of the given state.
   In detail it prints out the number of neighbors, the number of 
   improving/non-improving/worsening moves and their percentages.

   @param st the state to inspect
*/
template <class Input, class Output, class State, class Move, typename CFtype>
void MoveTester<Input,Output,State,Move,CFtype>::PrintNeighborhoodStatistics(const State& st, std::ostream& os) const
{
  unsigned int neighbors = 0, improving_neighbors = 0,
    worsening_neighbors = 0, non_improving_neighbors = 0;
  Move mv;
  CFtype mv_cost;

  ne.FirstMove(st,mv);
  do
    {
      neighbors++;
      mv_cost = ne.DeltaCostFunction(st,mv);
      if (mv_cost < 0)
	improving_neighbors++;
      else if (mv_cost > 0)
	worsening_neighbors++;
      else
	non_improving_neighbors++;
      ne.NextMove(st,mv);
    }
  while (!ne.LastMoveDone(st,mv));
  os << "Neighborhood size: " <<  neighbors << std::endl
     << "   improving moves: " << improving_neighbors << " ("
     << (100.0*improving_neighbors)/neighbors << "%)" << std::endl
     << "   worsening moves: " << worsening_neighbors << " ("
     << (100.0*worsening_neighbors)/neighbors << "%)" << std::endl
     << "   sideways moves: " << non_improving_neighbors << " ("
     << (100.0*non_improving_neighbors)/neighbors << "%)" << std::endl;
}

template <class Input, class Output, class State, class Move, typename CFtype>
void MoveTester<Input,Output,State,Move,CFtype>::PrintAllNeighbors(const State& st, std::ostream& os) const
{
  Move mv;
  ne.FirstMove(st,mv);
  do
    {
      os << mv << ' ' << ne.DeltaCostFunction(st,mv) << std::endl;
      ne.NextMove(st,mv);
    }
  while (!ne.LastMoveDone(st,mv));
}

template <class Input, class Output, class State, class Move, typename CFtype>
void MoveTester<Input,Output,State,Move,CFtype>::CheckMoveIndependence(const State& st, 
								       std::ostream& os) const
{
  Move mv;
  std::vector<State> states;
  std::vector<Move> moves;
  unsigned repeat_states = 0, null_moves = 0, all_moves = 0;
  int index;
  State st1 = st;
  ne.FirstMove(st1,mv);
  states.push_back(st1);
  moves.push_back(mv);

  do
    {
      ne.NextMove(st,mv);
      st1 = st;
      ne.MakeMove(st1, mv);
      if (st1 == st)
	{
	  os << "Null move " << mv << std::endl;
	  null_moves++;
	}	      	      
      else if ((index = this->sm.IsMember(st1,states)) != -1)
	{
	  os << "Repeated state for moves " << moves[index] << " and " << mv << std::endl;
	  repeat_states++;
	}
      else
	{
	  states.push_back(st1);
	  moves.push_back(mv);
	}
      if (all_moves % 100 == 0) std::cerr << '.'; // show that it is alive
      all_moves++;
    }
  while (!ne.LastMoveDone(st, mv));

  os << "Number of moves: " << all_moves << std::endl;
  if (repeat_states == 0)
    os << "No repeated states" << std::endl;
  else
    os << "There are " << repeat_states << " repeated states" << std::endl;
  if (null_moves == 0)
    os << "No null moves" << std::endl;
  else
    os << "There are " << null_moves << " null moves" << std::endl;
}

#endif /*MOVETESTER_HH_*/
