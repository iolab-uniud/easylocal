#ifndef MOVETESTER_HH_
#define MOVETESTER_HH_

#include "ComponentTester.hh"
#include "../helpers/OutputManager.hh"
#include "../helpers/NeighborhoodExplorer.hh"

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
protected:
  int Member(const State& s, const vector<State>& v);
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
{ }


/**
   Outputs the menu options.
*/
template <class Input, class Output, class State, class Move, typename CFtype>
void MoveTester<Input,Output,State,Move,CFtype>::ShowMenu()
{
    std::cout << "Move Menu: " << std::endl
    << "     (1)  Best" << std::endl
    << "     (2)  Random" << std::endl
    << "     (3)  Input" << std::endl
    << "     (4)  Print Neighborhood statistics" << std::endl
    << "     (5)  Check Move Info" << std::endl 
    << "     (6)  Check Move Indenpendence" << std::endl 
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
        ne.ReadMove(mv);
        break;
    case 4:
        ne.PrintNeighborhoodStatistics(st);
        break;
    case 5:
        {
            char ch;
            std::cout << "Input move (i) or check all moves (a)? ";
            std::cin >> ch;
            if (tolower(ch) == 'i')
            {
	      std::cout << "Input move : ";
	      std::cin >> mv;
	      std::cout << "Move info" << std::endl;
	      ne.PrintMoveInfo(st,mv);
            }
	    else // check all moves
	      {
		unsigned move_count = 0;
		State st1 = st;
		ne.FirstMove(st,mv);
		double error;
		do
		  {
		    move_count++;
		    ne.NextMove(st,mv);
		    st1 = st;
		    ne.MakeMove(st1, mv);
		    error = this->sm.CostFunction(st1) - ne.DeltaCostFunction(st, mv) - this->sm.CostFunction(st);
		    if (move_count % 100 == 0) std::cerr << '.'; // show that it is alive
		  }
		while(IsZero(error) && !ne.LastMoveDone(st, mv));

		if (!ne.LastMoveDone(st, mv))
		  {
		    std::cout << std::endl << "Error: Move n. " << move_count << ", Info" << std::endl;
		    ne.PrintMoveInfo(st,mv);
		  }
		else
		  std::cout << std::endl << "No error found (for " << move_count << " moves)!" << std::endl;
	      }
            break;
        }
    case 6:
      {
	vector<State> states;
	vector<Move> moves;
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
		std::cout << "Null move " << mv << std::endl;
		null_moves++;
	      }	      	      
	    else if ((index = Member(st1,states)) != -1)
	      {
		std::cout << "Repeated state for moves " << moves[index] << " and " << mv << std::endl;
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

	std::cout << "Number of moves: " << all_moves << std::endl;
	if (repeat_states == 0)
	  std::cout << "No repeated states" << std::endl;
	else
	  std::cout << "There are " << repeat_states << " repeated states" << std::endl;
	if (null_moves == 0)
	  std::cout << "No null moves" << std::endl;
	else
	  std::cout << "There are " << null_moves << " null moves" << std::endl;
	break;
      }
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
int MoveTester<Input,Output,State,Move,CFtype>::Member(const State& s, const vector<State>& v)
{
  for (unsigned i = 0; i < v.size(); i++)
    if (s == v[i])
      return i;
  return -1;
}

#endif /*MOVETESTER_HH_*/
