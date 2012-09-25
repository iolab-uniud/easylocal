#if !defined(_MOVE_TESTER_HH_)
#define _MOVE_TESTER_HH_

#include <testers/ComponentTester.hh>
#include <testers/Tester.hh>
#include <helpers/OutputManager.hh>
#include <helpers/NeighborhoodExplorer.hh>
#include <helpers/TabuListManager.hh>
#include <map>
#include <iterator>
#include <math.h>

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
  MoveTester(const Input& in,
             StateManager<Input,State,CFtype>& e_sm,
             OutputManager<Input,Output,State,CFtype>& e_om,
             NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
             TabuListManager<State,Move,CFtype>& e_tlm,
             std::string name, std::ostream& o = std::cout);
  MoveTester(const Input& in,
             StateManager<Input,State,CFtype>& e_sm,
             OutputManager<Input,Output,State,CFtype>& e_om,
             NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
             std::string name,
             Tester<Input,Output,State,CFtype>& t,
             std::ostream& o = std::cout);
  MoveTester(const Input& in,
             StateManager<Input,State,CFtype>& e_sm,
             OutputManager<Input,Output,State,CFtype>& e_om,
             NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
             TabuListManager<State,Move,CFtype>& e_tlm,
             std::string name,
             Tester<Input,Output,State,CFtype>& t,
             std::ostream& o = std::cout);
  void RunMainMenu(State& st);
  void PrintNeighborhoodStatistics(const State& st) const;
  void PrintAllNeighbors(const State& st) const;
  void CheckNeighborhoodCosts(const State& st) const;
  void PrintMoveCosts(const State& st,  const Move& mv) const;
  void CheckMoveIndependence(const State& st) const;
  void CheckRandomMoveDistribution(const State& st) const;
  void CheckTabuStrength(const State& st) const;
  void CheckCandidateInitialTemperature() const;
  unsigned int Modality() const;
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
  TabuListManager<State,Move,CFtype>* tlm; /**< A reference to the
                                            attached tabu list manager (if any). */
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
{ tlm = NULL; }

template <class Input, class Output, class State, class Move, typename CFtype>
MoveTester<Input,Output,State,Move,CFtype>::MoveTester(const Input& i,
                                                       StateManager<Input,State,CFtype>& e_sm,
                                                       OutputManager<Input,Output,State,CFtype>& e_om,
                                                       NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                                                       TabuListManager<State,Move,CFtype>& e_tlm,
                                                       std::string name, std::ostream& o)
: ComponentTester<Input,Output,State,CFtype>(name), in(i), out(i), sm(e_sm), om(e_om), ne(e_ne), tlm(&e_tlm), os(o)
{}

template <class Input, class Output, class State, class Move, typename CFtype>
MoveTester<Input,Output,State,Move,CFtype>::MoveTester(const Input& i,
                                                       StateManager<Input,State,CFtype>& e_sm,
                                                       OutputManager<Input,Output,State,CFtype>& e_om,
                                                       NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                                                       std::string name,
                                                       Tester<Input,Output,State,CFtype>& t,
                                                       std::ostream& o)
: ComponentTester<Input,Output,State,CFtype>(name), in(i), out(i), sm(e_sm), om(e_om), ne(e_ne), os(o)
{ tlm = NULL; t.AddMoveTester(*this); }

template <class Input, class Output, class State, class Move, typename CFtype>
MoveTester<Input,Output,State,Move,CFtype>::MoveTester(const Input& i,
                                                       StateManager<Input,State,CFtype>& e_sm,
                                                       OutputManager<Input,Output,State,CFtype>& e_om,
                                                       NeighborhoodExplorer<Input,State,Move,CFtype>& e_ne,
                                                       TabuListManager<State,Move,CFtype>& e_tlm,
                                                       std::string name,
                                                       Tester<Input,Output,State,CFtype>& t,
                                                       std::ostream& o)
: ComponentTester<Input,Output,State,CFtype>(name), in(i), out(i), sm(e_sm), om(e_om), ne(e_ne), tlm(&e_tlm), os(o)
{ t.AddMoveTester(*this); }


template <class Input, class Output, class State, class Move, typename CFtype>
void MoveTester<Input,Output,State,Move,CFtype>::RunMainMenu(State& st)
{
  bool show_state;
  do
  {
    ShowMenu();
    if (choice != 0)
    {
      std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
      show_state = ExecuteChoice(st);
      secs duration = std::chrono::duration_cast<secs>(std::chrono::high_resolution_clock::now() - start);
      if (show_state)
      {
        om.OutputState(st,out);
        os << "CURRENT SOLUTION " << std::endl << out << std::endl;
        os << "CURRENT COST : " << sm.CostFunction(st) << std::endl;
      }
      os << "ELAPSED TIME : " << duration.count() << 's' << std::endl;
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
  << "     (9)  Check Move Independence" << std::endl
  << "    (10)  Check Random Move Distribution" << std::endl
  << "    (11)  Check Candidate Initial Temperature for SA" << std::endl;
  if (tlm != NULL)
    os << "    (12)  Chech Tabu Strength" << std::endl;
  os << "     (0)  Return to Main Menu" << std::endl
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
  try
  {
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
      case 10:
        CheckRandomMoveDistribution(st);
        break;
      case 11:
        CheckCandidateInitialTemperature();
        break;
      case 12:
        CheckTabuStrength(st);
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
  }
  catch (EmptyNeighborhood e)
  {
    os << "Empty neighborhood" << std::endl;
  }
  
  return false;
}

template <class Input, class Output, class State, class Move, typename CFtype>
void MoveTester<Input,Output,State,Move,CFtype>::PrintMoveCosts(const State& st, const Move& mv) const
{
  CFtype delta_cost, total_delta_hard_cost = 0, total_delta_soft_cost = 0;
  State st1 = st;
  unsigned int i;
  
  // it's a tester, so we can avoid optimizations
  os << "Move : " << mv << std::endl;
  ne.MakeMove(st1,mv);
  
  // process all delta cost components
  for (i = 0; i < ne.DeltaCostComponents(); i++)
  {
    DeltaCostComponent<Input,State,Move,CFtype>& dcc = ne.GetDeltaCostComponent(i);
    delta_cost = dcc.DeltaCost(st,mv);
    os << "  " << i << ". " << dcc.name << " : " <<  delta_cost;
    
    // print * or not, add up to right variable
    if (dcc.IsHard()) 
    {
      total_delta_hard_cost += delta_cost;
      os << "*";
    } else
      total_delta_soft_cost = delta_cost;
    os << std::endl;
  }
  
  // process all cost components
  for(i = 0; i < ne.CostComponents(); i++)
  {
    CostComponent<Input,State,CFtype>& cc = ne.GetCostComponent(i);
    delta_cost = cc.Weight() * (cc.ComputeCost(st1) - cc.ComputeCost(st));
    os << "  " << i << ". " << cc.name << " : " <<  delta_cost;
    
    // print * or not, add up to right variable
    if (cc.IsHard()) 
    {
      total_delta_hard_cost += delta_cost;
      os << "*";
    } else
      total_delta_soft_cost = delta_cost;
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
  unsigned int move_count = 0;
  CFtype error, error_cc, delta_cost, cost, cost1;
  State st1 = st;
  bool error_found = false, not_last_move = true;
  ne.FirstMove(st, mv);
  do
  {
    move_count++;
    
    ne.MakeMove(st1, mv);
    error = this->sm.CostFunction(st1) - ne.DeltaCostFunction(st, mv) - this->sm.CostFunction(st);
    if (!IsZero(error))
    {
      error_found = true;
      os << std::endl << "Error: Move n. " << move_count << ", " << mv << ", Total error = " << error <<  ", Info" << std::endl;
      // only implemented delta can be buggy
      for (unsigned int i = 0; i < ne.DeltaCostComponents(); i++)
      {      
        DeltaCostComponent<Input,State,Move,CFtype>& dcc = ne.GetDeltaCostComponent(i);
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
  unsigned int i;
  Move mv;
  CFtype mv_cost, delta_cost;
  double total_positive_cost = 0.0;
  
  
  std::vector<std::pair<CFtype,CFtype> > min_max_costs(ne.DeltaCostComponents());
  
  ne.FirstMove(st,mv);
  // only implemented delta can be buggy
  for (i = 0; i < ne.DeltaCostComponents(); i++)
  {
    DeltaCostComponent<Input,State,Move,CFtype>& dcc = ne.GetDeltaCostComponent(i);
    min_max_costs[i].first =  min_max_costs[i].second = dcc.DeltaCost(st, mv);  
  }
  
  do
  {
    neighbors++;
    mv_cost = ne.DeltaCostFunction(st,mv);
    if (mv_cost < 0)
      improving_neighbors++;
    else if (mv_cost > 0)
    {
      worsening_neighbors++;
      total_positive_cost += mv_cost;
    }
    else
      non_improving_neighbors++;
    // only implemented delta can be buggy
    for (i = 0; i < ne.DeltaCostComponents(); i++)
    {      
      DeltaCostComponent<Input,State,Move,CFtype>& dcc = ne.GetDeltaCostComponent(i);
      delta_cost = dcc.DeltaCost(st, mv);  
      if (delta_cost < min_max_costs[i].first)
        min_max_costs[i].first = delta_cost;
      else if (delta_cost > min_max_costs[i].second)
        min_max_costs[i].second = delta_cost;
    }
  }
  while (ne.NextMove(st,mv));
  
  os << "Neighborhood size: " <<  neighbors << std::endl
  << "   improving moves: " << improving_neighbors << " ("
  << (100.0*improving_neighbors)/neighbors << "%)" << std::endl
  << "   worsening moves: " << worsening_neighbors << " ("
  << (100.0*worsening_neighbors)/neighbors << "%), average cost: " << total_positive_cost/neighbors << std::endl
  << "   sideways moves: " << non_improving_neighbors << " ("
  << (100.0*non_improving_neighbors)/neighbors << "%)" << std::endl;
  
  os << "Min and max component costs:" << std::endl;
  for (i = 0; i < ne.DeltaCostComponents(); i++)
    os << "  " << i << ". " << ne.GetDeltaCostComponent(i).name << " : Min = " << min_max_costs[i].first << ", Max = "  << min_max_costs[i].second << std::endl; 
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
void MoveTester<Input,Output,State,Move,CFtype>::CheckRandomMoveDistribution(const State& st) const
{
  Move mv;  
  std::map<Move, unsigned int> frequency;
  typename std::map<Move,unsigned int>:: iterator it;  
  
  unsigned int trials = 0, tot_trials, rounds;
  double dev = 0;
  
  ne.FirstMove(st,mv);
  do
  {
    frequency[mv] = 0;
  }
  while (ne.NextMove(st,mv));
  
  os << "The neighborhood has " << frequency.size() << " members." << std::endl;
  os << "How many rounds do you want to test: ";
  std::cin >> rounds;
  
  tot_trials = frequency.size() * rounds;
  while (trials < tot_trials)
  {
    ne.RandomMove(st,mv);
    if (frequency.find(mv) != frequency.end())
    {
      frequency[mv]++;
    }
    else
      os << "Random move not in neighborhood " << mv << std::endl;
    trials++;
    if (trials % frequency.size() == 0)
      std::cerr << '.';
  }
  
  // Compute the standard deviation
  for (it = frequency.begin(); it != frequency.end(); it++)
  {
    dev += pow((double)(*it).second, 2);
  }
  
  dev = sqrt(fabs(dev/frequency.size() - pow(double(rounds), 2))); 
  
  double error = 0;
  
  os << "Outlier moves [move frequency]:" << std::endl;
  for (it = frequency.begin(); it != frequency.end(); it++)
  {
    if (fabs((*it).second - double(rounds)) > 3*dev || (*it).second == 0)
    {
      error++;
      os << it->first << " " << it->second/double(rounds) << std::endl;
    }
  }
  std::cerr << "Deviation of move frequency: " << dev << std::endl;
  std::cerr << "Percentage of outliers " << 100 * error/frequency.size() << '%' << std::endl;
}

template <class Input, class Output, class State, class Move, typename CFtype>
void MoveTester<Input,Output,State,Move,CFtype>::CheckMoveIndependence(const State& st) const
{
  Move mv;
  std::vector<std::pair<Move,State> > reached_states;
  unsigned int repeat_states = 0, null_moves = 0, all_moves = 1, i;
  bool repeated_state;
  State st1 = st;
  ne.FirstMove(st1,mv);
  ne.MakeMove(st1, mv);
  if (st1 == st)
  {
    os << "Null move " << mv << std::endl;
    null_moves++;
  }
  else
  {
    reached_states.push_back(std::make_pair(mv,st1));
  }
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
	      {
          repeated_state = true;
          break;
	      }
      if (repeated_state)
	    {
	      os << "Repeated state for moves " <<  reached_states[i].first << " and " << mv << std::endl;
	      repeat_states++;
	    }
      else
	    {
	      reached_states.push_back(std::make_pair(mv,st1));
	    }
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

template <class Input, class Output, class State, class Move, typename CFtype>
void MoveTester<Input,Output,State,Move,CFtype>::CheckTabuStrength(const State& st) const
{
  Move mv1, mv2;
  State st1 = st;
  long long unsigned moves = 0, pairs = 0, inverse_pairs = 0;
  std::vector<long long unsigned> moves_per_type(ne.Modality(), 0);
  ne.FirstMove(st,mv1);
  do
  {
    for (unsigned int i = 0; i < ne.Modality(); i++)
      moves_per_type[i] = 0;
    moves_per_type[ne.MoveModality(mv1)]++;
    st1 = st;
    ne.MakeMove(st1, mv1);
    ne.FirstMove(st1, mv2);    
    moves++;    
    do 
    {
      moves_per_type[ne.MoveModality(mv2)]++;
      pairs++;
      if (tlm->Inverse(mv1, mv2))
	    {
	      std::cerr << mv1 << " -- " << mv2 << std::endl;
	      inverse_pairs++;	        
	    }
      if (pairs % 100000 == 0) 
        std::cerr << '.'; // print dots to show that it is alive
    }
    while (ne.NextMove(st1, mv2));
    std::cerr << ne.MoveModality(mv1) << ':';
    for (unsigned int i = 0; i < ne.Modality(); i++)
      std::cerr << moves_per_type[i] << (i < (ne.Modality() - 1) ? "/" : "");
    std::cerr << std::endl;
  }
  while (ne.NextMove(st,mv1));
  os << std::endl
  << "Moves : " << moves << ", total pairs : " << pairs 
  << ", inverse pairs : " << inverse_pairs << std::endl
  << "Tabu ratio : " << double(inverse_pairs)/pairs * 100 << "%" << std::endl;
  os << "Non-inverse moves " << double(pairs - inverse_pairs)/moves << std::endl;
}



template <class Input, class Output, class State, class Move, typename CFtype>
void MoveTester<Input,Output,State,Move,CFtype>::CheckCandidateInitialTemperature() const
{
  const unsigned int init_states = 100, samples = 1000;
  unsigned int i, j;
  CFtype cost_value, max_cost_value = (CFtype) 0;
  double mean, square_mean, variance, mean_variance = 0.0;
  
  // Compute a start temperature by sampling the search space and computing the variance
  // according to [van Laarhoven and Aarts, 1987] (allow an acceptance ratio of approximately 80%)
  State sample_state(this->in);
  Move mv;
  
  for (i = 0; i < init_states; i++)
  {
    mean = 0.0;
    square_mean = 0.0;
    this->sm.RandomState(sample_state);
    for (j = 0; j < samples; j++)
    {
      this->ne.RandomMove(sample_state, mv);
      cost_value = this->ne.DeltaCostFunction(sample_state, mv);
      if (cost_value > max_cost_value)
        max_cost_value = cost_value;
      mean += cost_value;
      square_mean += cost_value * cost_value;
    }
    mean /= samples;
    variance = square_mean - mean * mean;
    mean_variance += variance;
  }
  mean_variance /= init_states;
  
  mean_variance /= 10e04; // scaling
  
  os << "Start temperature 1 = " << mean_variance << std::endl;
  os << "Start temperature 2 = " << max_cost_value << " (" << mean_variance/max_cost_value << ")" << std::endl;
}

template <class Input, class Output, class State, class Move, typename CFtype>
unsigned int MoveTester<Input,Output,State,Move,CFtype>::Modality() const
{ return ne.Modality(); }

#endif // _MOVE_TESTER_HH_
