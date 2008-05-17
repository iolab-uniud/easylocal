#if !defined(_MOVE_TESTER_HH_)
#define _MOVE_TESTER_HH_

#include <testers/ComponentTester.hh>
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
	     std::string name, std::ostream& o = std::cout);
  void RunMainMenu(State& st);
  void PrintNeighborhoodStatistics(const State& st) const;
  void PrintAllNeighbors(const State& st) const;
  void CheckNeighborhoodCosts(const State& st) const;
  void PrintMoveCosts(const State& st,  const Move& mv) const;
  void CheckMoveIndependence(const State& st) const;
protected:
  void ShowMenu();
  bool ExecuteChoice(State& st);
  const Input& in;
  Output out;   /**< The output object. */
  StateManager<Input,State,CFtype>& sm; /**< A pointer to the attached
					   state manager. */
  OutputManager<Input,Output,State,CFtype>& om; /**< A pointer to the attached
						   output manager. */
  NeighborhoodExplorer<Input,State,Move,CFtype>& ne; /**< A reference to the
							attached neighborhood
							explorer. */
  unsigned int choice;   /**< The option currently chosen from the menu. */
  std::ostream& os;
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
MoveTester<Input,Output,State,Move,CFtype>::MoveTester(const Input& i,
						       StateManager<Input,State,CFtype>& e_sm,
						       OutputManager<Input,Output,State,CFtype>& e_om,
						       NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
						       std::string name, std::ostream& o)
  : ComponentTester<Input,Output,State,CFtype>(name), in(i), out(i), sm(e_sm), om(e_om), ne(e_ne), os(o)
{}


template <class Input, class Output, class State, class Move, typename CFtype>
void MoveTester<Input,Output,State,Move,CFtype>::RunMainMenu(State& st)
{
    bool show_state;
    do
    {
        ShowMenu();
        if (choice != 0)
        {
          Chronometer chrono;
          chrono.Start();
          show_state = ExecuteChoice(st);
          chrono.Stop();
          if (show_state)
          {
            om.OutputState(st,out);
            os << "CURRENT SOLUTION " << std::endl << out << std::endl;
            os << "CURRENT COST : " << sm.CostFunction(st) << std::endl;
          }
          os << "ELAPSED TIME : " << chrono.TotalTime() << 's' << std::endl;
        }
    }
    while (choice != 0);
    os << "Leaving " << this->name << " menu" << std::endl;
}
 
/**
   Outputs the menu options.
*/
template <class Input, class Output, class State, class Move, typename CFtype>
void MoveTester<Input,Output,State,Move,CFtype>::ShowMenu()
{
  os << "Move Menu: " << std::endl
     << "     (1)  Perform Best Move" << std::endl
     << "     (2)  Perform Random Move" << std::endl
     << "     (3)  Perform Input Move" << std::endl
     << "     (4)  Print All Neighbors" << std::endl
     << "     (5)  Print Neighborhood Statistics" << std::endl
     << "     (6)  Print Random Move Cost" << std::endl 
     << "     (7)  Print Input Move Cost" << std::endl 
     << "     (8)  Check Neighborhood Costs" << std::endl 
     << "     (9)  Check Move Indenpendence" << std::endl 
     << "     (0)  Return to Main Menu" << std::endl
     << " Your choice: ";
  std::cin >> choice;
}

/**
   Execute the menu choice on the given state.

   @param st the current state
*/
template <class Input, class Output, class State, class Move, typename CFtype>
bool MoveTester<Input,Output,State,Move,CFtype>::ExecuteChoice(State& st)
{
  Move mv;
  switch(choice)
    {
    case 1:
      ne.BestMove(st,mv);
      break;
    case 2:
      ne.RandomMove(st,mv);
      break;
    case 3:
      os << "Input move : ";
      std::cin >> mv;
      break;
    case 4:
      PrintAllNeighbors(st);
      break;
    case 5:
      PrintNeighborhoodStatistics(st);
      break;
    case 6:
      ne.RandomMove(st,mv);
      PrintMoveCosts(st,mv);
      break;
    case 7:
      do 
	{
	  os << "Input move : ";
	  std::cin >> mv;
	  if (!ne.FeasibleMove(st,mv))
	    os << "Move " << mv << " is infeasible " << std::endl;
	}
      while (!ne.FeasibleMove(st,mv));
      PrintMoveCosts(st,mv);
      break;
    case 8:
      CheckNeighborhoodCosts(st);
      break;
    case 9:
      CheckMoveIndependence(st);
      break;
    default:
      os << "Invalid choice" << std::endl;
    }
  if (choice == 1 || choice == 2 || choice == 3)
    {
      os << "Move : " << mv << std::endl;
      if (ne.FeasibleMove(st,mv))
	{
	  ne.MakeMove(st,mv);
	  return true;
	}
      else
	os << "Infeasible move!" << std::endl;
    }
  
  return false;
}
 
template <class Input, class Output, class State, class Move, typename CFtype>
void MoveTester<Input,Output,State,Move,CFtype>::PrintMoveCosts(const State& st, const Move& mv) const
{
  CFtype delta_cost, total_delta_hard_cost = 0, total_delta_soft_cost = 0;
  State st1 = st;

  os << "Move : " << mv << std::endl;
  ne.MakeMove(st1,mv);

  for (unsigned i = 0; i < ne.DeltaCostComponents(); i++)
    {
      AbstractDeltaCostComponent<Input,State,Move,CFtype>& dcc = ne.DeltaCostComponent(i);
      if (dcc.IsDeltaImplemented())
	delta_cost = static_cast<FilledDeltaCostComponent<Input,State,Move,CFtype>& >(dcc).DeltaCost(st, mv);        
      else
	delta_cost = static_cast<EmptyDeltaCostComponent<Input,State,Move,CFtype>& >(dcc).DeltaCost(st, st1);
      os << "  " << i << ". " << dcc.name << " : " <<  delta_cost;
      if (dcc.IsHard())
	{
	  total_delta_hard_cost += delta_cost;
	  os << '*';
	}
      else
	total_delta_soft_cost += delta_cost;
      os << std::endl;
    }
  os << "Total Delta Violations : " << total_delta_hard_cost << std::endl;
  os << "Total Delta Objective : " << total_delta_soft_cost << std::endl;
  os << "Total Delta Cost : " << HARD_WEIGHT * total_delta_hard_cost + total_delta_soft_cost << std::endl;
}

template <class Input, class Output, class State, class Move, typename CFtype>
void MoveTester<Input,Output,State,Move,CFtype>::CheckNeighborhoodCosts(const State& st) const
{
  Move mv;
  unsigned move_count = 0;
  CFtype error, error_cc, delta_cost, cost, cost1;
  State st1 = st;
  bool error_found = false, not_last_move;
  ne.FirstMove(st, mv);
  do
    {
      move_count++;
      ne.MakeMove(st1, mv);
      error = this->sm.CostFunction(st1) - ne.DeltaCostFunction(st, mv) - this->sm.CostFunction(st);
      if (!IsZero(error))
	{
	  error_found = true;
	  os << std::endl << "Error: Move n. " << move_count << ", " << mv << ", Info" << std::endl;
	  for (unsigned i = 0; i < ne.DeltaCostComponents(); i++)
	    {
	      if (ne.DeltaCostComponent(i).IsDeltaImplemented()) // only implemented delta can be buggy
				{
					FilledDeltaCostComponent<Input,State,Move,CFtype>& dcc = (FilledDeltaCostComponent<Input,State,Move,CFtype>&) ne.DeltaCostComponent(i);
					CostComponent<Input,State,CFtype>& cc = dcc.GetCostComponent();
					delta_cost = dcc.DeltaCost(st, mv);        
					cost = cc.Cost(st);
					cost1 = cc.Cost(st1);
					error_cc = cost - cost1 + delta_cost;
					if (!IsZero(error_cc))
					{		    
						os << "  " << i << ". " << dcc.name << " : Initial = " << cost << ", final = " 
						<< cost1 << ", delta computed = " << delta_cost << " (error = " << error_cc << ")" << std::endl;		      
					}
				}
	    }
	  os << "Press enter to continue " << std::endl;
	  std::cin.get();
	}          
      if (move_count % 100 == 0) 
	std::cerr << '.'; // print dots to show that it is alive
      not_last_move = ne.NextMove(st, mv);
      st1 = st;
    }
  while(not_last_move);
  
  if (!error_found)
    os << std::endl << "No error found (for " << move_count << " moves)!" << std::endl;
}

/**
   Outputs some statistics about the neighborhood of the given state.
   In detail it prints out the number of neighbors, the number of 
   improving/non-improving/worsening moves and their percentages.

   @param st the state to inspect
*/
template <class Input, class Output, class State, class Move, typename CFtype>
void MoveTester<Input,Output,State,Move,CFtype>::PrintNeighborhoodStatistics(const State& st) const
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
    }
  while (ne.NextMove(st,mv));
  os << "Neighborhood size: " <<  neighbors << std::endl
     << "   improving moves: " << improving_neighbors << " ("
     << (100.0*improving_neighbors)/neighbors << "%)" << std::endl
     << "   worsening moves: " << worsening_neighbors << " ("
     << (100.0*worsening_neighbors)/neighbors << "%)" << std::endl
     << "   sideways moves: " << non_improving_neighbors << " ("
     << (100.0*non_improving_neighbors)/neighbors << "%)" << std::endl;
}

template <class Input, class Output, class State, class Move, typename CFtype>
void MoveTester<Input,Output,State,Move,CFtype>::PrintAllNeighbors(const State& st) const
{
  Move mv;
  ne.FirstMove(st,mv);
  do
    {
      os << mv << ' ' << ne.DeltaCostFunction(st,mv) << std::endl;
    }
  while (ne.NextMove(st,mv));
}

template <class Input, class Output, class State, class Move, typename CFtype>
void MoveTester<Input,Output,State,Move,CFtype>::CheckMoveIndependence(const State& st) const
{
  Move mv;
  std::vector<std::pair<Move,State> > reached_states;
  unsigned repeat_states = 0, null_moves = 0, all_moves = 1, i;
  bool repeated_state;
  State st1 = st;
  ne.FirstMove(st1,mv);
  reached_states.push_back(make_pair(mv,st1));

  while (ne.NextMove(st,mv))
    {
      st1 = st;
      ne.MakeMove(st1, mv);
      if (st1 == st)
	{
	  os << "Null move " << mv << std::endl;
	  null_moves++;
	}	      	      
      else 
	{
	  repeated_state = false;
	  for (i = 0; i < reached_states.size(); i++)
	    if (st1 == reached_states[i].second)
	      repeated_state = true;
	  if (repeated_state)
	    {
	      os << "Repeated state for moves " <<  reached_states[i].first << " and " << mv << std::endl;
	      repeat_states++;
	    }
	  else
	    reached_states.push_back(make_pair(mv,st1));
	}
      if (all_moves % 100 == 0) 
	std::cerr << '.'; // print dots to show that it is alive
      all_moves++;
    }

  os << std::endl << "Number of moves: " << all_moves << std::endl;
  if (repeat_states == 0)
    os << "No repeated states" << std::endl;
  else
    os << "There are " << repeat_states << " repeated states" << std::endl;
  if (null_moves == 0)
    os << "No null moves" << std::endl;
  else
    os << "There are " << null_moves << " null moves" << std::endl;
}

#endif // define _MOVE_TESTER_HH_
