#pragma once

#include <map>
#include <iterator>
#include <cmath>
#include <chrono>

#include "easylocal/testers/componenttester.hh"
#include "easylocal/testers/tester.hh"
#include "easylocal/helpers/outputmanager.hh"
#include "easylocal/helpers/neighborhoodexplorer.hh"
#include "easylocal/helpers/coststructure.hh"

namespace EasyLocal {
  
  namespace Debug {
    
    /** A Move Tester allows to test the behavior of a given
     neighborhood explorer.
     @ingroup Testers
     */
    template <class Input, class Output, class State, class Move, class CostStructure = Core::DefaultCostStructure<int>>
    class MoveTester
    : public ComponentTester<Input, Output, State, CostStructure>, public ChoiceReader
    {
      typedef typename CostStructure::CFtype CFtype;
    public:
      MoveTester(const Input& in,
                 Core::StateManager<Input, State, CostStructure>& sm,
				 Core::OutputManager<Input, Output, State>& om,
		         Core::NeighborhoodExplorer<Input, State, Move, CostStructure>& ne,
                 std::string name,
                 Tester<Input, Output, State, CostStructure>& t,
                 std::ostream& o = std::cout);
      void RunMainMenu(State& st);
      void PrintNeighborhoodStatistics(const State& st) const;
      void PrintAllNeighbors(const State& st) const;
      void CheckNeighborhoodCosts(const State& st) const;
      void PrintMoveCosts(const State& st, const EvaluatedMove<Move, CostStructure>& em) const;
      void CheckMoveIndependence(const State& st) const;
      void CheckRandomMoveDistribution(const State& st) const;
      size_t Modality() const;
      void SetTolerance(double t);
    protected:
      void ShowMenu();
      bool ExecuteChoice(State& st);
      const Input& in;
      Output out;   /**< The output object. */
      Core::StateManager<Input, State, CostStructure>& sm; /**< A pointer to the attached  state manager. */
      Core::OutputManager<Input, Output, State>& om; /**< A pointer to the attached output manager. */
      Core::NeighborhoodExplorer<Input, State, Move, CostStructure>& ne; /**< A reference to the attached neighborhood explorer. */
      int choice;   /**< The option currently chosen from the menu. */
      std::ostream& os;
      double tolerance;
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    template <class Input, class Output, class State, class Move, class CostStructure>
    MoveTester<Input, Output, State, Move, CostStructure>::MoveTester(const Input& i,
                                                               Core::StateManager<Input, State, CostStructure>& sm,
                                                               Core::OutputManager<Input, Output, State>& om,
                                                               Core::NeighborhoodExplorer<Input, State, Move, CostStructure>& ne,
                                                               std::string name,
                                                               Tester<Input, Output, State, CostStructure>& t,
                                                               std::ostream& o)
    : ComponentTester<Input, Output, State, CostStructure>(name), in(i), out(i), sm(sm), om(om), ne(ne), os(o), tolerance(std::numeric_limits<CFtype>::epsilon())
    {
      t.AddMoveTester(*this);
    }
    
    
    template <class Input, class Output, class State, class Move, class CostStructure>
    void MoveTester<Input, Output, State, Move, CostStructure>::RunMainMenu(State& st)
    {
      bool show_state;
      do
      {
        ShowMenu();
        if (choice != 0)
        {
          std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
          show_state = ExecuteChoice(st);
          std::chrono::milliseconds duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start);
          if (show_state)
          {
            om.OutputState(st, out);
            os << "CURRENT SOLUTION " << std::endl << out << std::endl;
            os << "CURRENT COST : " << sm.CostFunctionComponents(st) << std::endl;
          }
          os << "ELAPSED TIME : " << duration.count() / 1000.0 << 's' << std::endl;
        }
      }
      while (choice != 0);
      os << "Leaving " << this->name << " menu" << std::endl;
    }
    
    /**
     Outputs the menu options.
     */
    template <class Input, class Output, class State, class Move, class CostStructure>
    void MoveTester<Input, Output, State, Move, CostStructure>::ShowMenu()
    {
      os << "Move Menu: " << std::endl
      << "     (1)  Perform Best Move" << std::endl
      << "     (2)  Perform First Improving Move" << std::endl
      << "     (3)  Perform Random Move" << std::endl
      << "     (4)  Perform Input Move" << std::endl
      << "     (5)  Print All Neighbors" << std::endl
      << "     (6)  Print Neighborhood Statistics" << std::endl
      << "     (7)  Print Random Move Cost" << std::endl
      << "     (8)  Print Input Move Cost" << std::endl
      << "     (9)  Check Neighborhood Costs" << std::endl
      << "    (10)  Check Move Independence" << std::endl
      << "    (11)  Check Random Move Distribution" << std::endl;
      os << "     (0)  Return to Main Menu" << std::endl
      << " Your choice: ";
      choice = this->ReadChoice(std::cin);
    }
    
    /**
     Execute the menu choice on the given state.
     
     @param st the current state
     */
    template <class Input, class Output, class State, class Move, class CostStructure>
    bool MoveTester<Input, Output, State, Move, CostStructure>::ExecuteChoice(State& st)
    {
      EvaluatedMove<Move, CostStructure> em;
      try
      {
        size_t explored;
        switch(choice)
        {
          case 1:
            em = ne.SelectBest(st, explored, [](const Move& mv, const CostStructure& cost) { return true; });
            break;
          case 2:
            em = ne.SelectFirst(st, explored, [](const Move& mv, const CostStructure& cost) { return cost.total < 0; });
            break;
          case 3:
            em = ne.RandomFirst(st, 1, explored, [](const Move& mv, const CostStructure& cost) { return true; });
            break;
          case 4:
            os << "Input move : ";
            std::cin >> em.move;
            break;
          case 5:
            PrintAllNeighbors(st);
            break;
          case 6:
            PrintNeighborhoodStatistics(st);
            break;
          case 7:
            em = ne.RandomFirst(st, 1, explored, [](const Move& mv, const CostStructure& cost) { return true; });
            PrintMoveCosts(st, em);
            break;
          case 8:
            os << "Input move : ";
            std::cin >> em.move;
            em.cost = ne.DeltaCostFunctionComponents(st, em.move);
            PrintMoveCosts(st, em);
            break;
          case 9:
            CheckNeighborhoodCosts(st);
            break;
          case 10:
            CheckMoveIndependence(st);
            break;
          case 11:
            CheckRandomMoveDistribution(st);
            break;
          default:
            os << "Invalid choice" << std::endl;
        }
        if (choice == 1 || choice == 2 || choice == 3 || choice == 4)
        {
          os << "Move : " << em.move << std::endl;
          if (!ne.FeasibleMove(st, em.move))
            os << "Move not feasible" << std::endl;
          else
            ne.MakeMove(st, em.move);
          return true;
        }
      }
      catch (EmptyNeighborhood e)
      {
        os << "Empty neighborhood" << std::endl;
      }
      
      return false;
    }
    
    template <class Input, class Output, class State, class Move, class CostStructure>
    void MoveTester<Input, Output, State, Move, CostStructure>::PrintMoveCosts(const State& st, const EvaluatedMove<Move, CostStructure>& em) const
    {
      // it's a tester, so we can avoid optimizations
      os << "Move : " << em.move << std::endl;
      
      // process all delta cost components
      for (size_t i = 0; i < sm.CostComponents(); i++)
      {
        const auto& cc = sm.GetCostComponent(i);
        os << "  " << i << ". " << cc.name << " : " <<  em.cost;
        
        // print * or not, add up to right variable
        if (cc.IsHard())
          os << "*";
        os << std::endl;
      }
      
      os << "Total Delta Violations : " << em.cost.violations << std::endl;
      os << "Total Delta Objective : " << em.cost.objective  << std::endl;
      os << "Total Delta Cost : " << em.cost.total << std::endl;
    }
    
    template <class Input, class Output, class State, class Move, class CostStructure>
    void MoveTester<Input, Output, State, Move, CostStructure>::CheckNeighborhoodCosts(const State& st) const
    {
      EvaluatedMove<Move, CostStructure> em;
      unsigned int move_count = 0;
      CostStructure error;
      CostStructure st_cost = this->sm.CostFunctionComponents(st), st1_cost;
      State st1 = st;
      bool error_found = false, not_last_move = true;
      ne.FirstMove(st, em.move);
      do
      {
        move_count++;
        
        ne.MakeMove(st1, em.move);
        em.cost = ne.DeltaCostFunctionComponents(st, em.move);
        st1_cost = this->sm.CostFunctionComponents(st1);
        error = st1_cost - em.cost - st_cost;
        for (size_t i = 0; i < sm.CostComponents(); i++)
        {
          if (!IsZero(error.all_components[i]) && std::abs(error.all_components[i]) > tolerance)
          {
            error_found = true;
            os << em.move << "  " << i << ". " <<  sm.GetCostComponent(i).name << ": " << st_cost.all_components[i] << std::showpos << em.cost.all_components[i] << std::noshowpos << "!="
            << st1_cost.all_components[i] << " (error = " << std::showpos << error.all_components[i] << ")" << std::noshowpos << std::endl;
            os << "Press enter to continue " << std::endl;
            std::cin.get();
          }
        }
        
        if (move_count % 100 == 0)
          std::cerr << '.'; // print dots to show that it is alive
        not_last_move = ne.NextMove(st, em.move);
        st1 = st;
        
      }
      while (not_last_move);
      
      if (!error_found)
        os << std::endl << "No error found (for " << move_count << " moves)!" << std::endl;
    }
    
    /**
     Outputs some statistics about the neighborhood of the given state.
     In detail it prints out the number of neighbors, the number of
     improving/non-improving/worsening moves and their percentages.
     
     @param st the state to inspect
     */
    template <class Input, class Output, class State, class Move, class CostStructure>
    void MoveTester<Input, Output, State, Move, CostStructure>::PrintNeighborhoodStatistics(const State& st) const
    {
      unsigned int neighbors = 0, improving_neighbors = 0,
      worsening_neighbors = 0, non_improving_neighbors = 0;
      EvaluatedMove<Move, CostStructure> em;
      double total_positive_cost = 0.0;
      
      
      std::vector<std::pair<CFtype, CFtype>> min_max_costs(sm.CostComponents());
      
      ne.FirstMove(st, em.move);
      
      do
      {
        neighbors++;
        em.cost = ne.DeltaCostFunctionComponents(st, em.move);

        if (em.cost.total < 0)
          improving_neighbors++;
        else if (em.cost.total > 0)
        {
          worsening_neighbors++;
          total_positive_cost += em.cost.total;
        }
        else
          non_improving_neighbors++;
        for (size_t i = 0; i < sm.CostComponents(); i++)
        {
          if (em.cost.all_components[i] < min_max_costs[i].first)
            min_max_costs[i].first = em.cost.all_components[i];
          else if (em.cost.all_components[i] > min_max_costs[i].second)
            min_max_costs[i].second = em.cost.all_components[i];
        }
      }
      while (ne.NextMove(st, em.move));
      
      os << "Neighborhood size: " <<  neighbors << std::endl
      << "   improving moves: " << improving_neighbors << " ("
      << (100.0 * improving_neighbors) / neighbors << "%)" << std::endl
      << "   worsening moves: " << worsening_neighbors << " ("
      << (100.0 * worsening_neighbors) / neighbors << "%), average cost: " << total_positive_cost / neighbors << std::endl
      << "   sideways moves: " << non_improving_neighbors << " ("
      << (100.0 * non_improving_neighbors) /neighbors << "%)" << std::endl;
      
      os << "Min and max component costs:" << std::endl;
      for (size_t i = 0; i < sm.CostComponents(); i++)
        os << "  " << i << ". " << sm.GetCostComponent(i).name << " : Min = " << min_max_costs[i].first << ", Max = "  << min_max_costs[i].second << std::endl;
    }
    
    template <class Input, class Output, class State, class Move, class CostStructure>
    void MoveTester<Input, Output, State, Move, CostStructure>::PrintAllNeighbors(const State& st) const
    {
      Move mv;
      ne.FirstMove(st, mv);
      do
      {
        os << mv << " " << ne.DeltaCostFunctionComponents(st, mv) << std::endl;
      }
      while (ne.NextMove(st, mv));
    }
    
    template <class Input, class Output, class State, class Move, class CostStructure>
    void MoveTester<Input, Output, State, Move, CostStructure>::CheckRandomMoveDistribution(const State& st) const
    {
      Move mv;
      std::map<Move, unsigned int> frequency;
      typename std::map<Move, unsigned int>:: iterator it;
      
      unsigned long int trials = 0, tot_trials, rounds;
      double dev = 0;
      
      ne.FirstMove(st, mv);
      do
      {
        frequency[mv] = 0;
      }
      while (ne.NextMove(st, mv));
      
      os << "The neighborhood has " << frequency.size() << " members." << std::endl;
      os << "How many rounds do you want to test: ";
      std::cin >> rounds;
      
      tot_trials = frequency.size() * rounds;
      while (trials < tot_trials)
      {
        ne.RandomMove(st, mv);
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
      for (it = frequency.begin(); it != frequency.end(); ++it)
      {
        dev += pow((double)(*it).second, 2);
      }
      
      dev = sqrt(fabs(dev/frequency.size() - pow(double(rounds), 2)));
      
      double error = 0;
      
      os << "Outlier moves [move frequency]:" << std::endl;
      for (it = frequency.begin(); it != frequency.end(); ++it)
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
    
    template <class Input, class Output, class State, class Move, class CostStructure>
    void MoveTester<Input, Output, State, Move, CostStructure>::CheckMoveIndependence(const State& st) const
    {
      Move mv;
      std::vector<std::pair<Move, State> > reached_states;
      unsigned int repeat_states = 0, null_moves = 0, all_moves = 1, i;
      bool repeated_state;
      State st1 = st;
      ne.FirstMove(st1, mv);
      ne.MakeMove(st1, mv);
      if (st1 == st)
      {
        os << "Null move " << mv << std::endl;
        null_moves++;
      }
      else
      {
        reached_states.push_back(std::make_pair(mv, st1));
      }
      while (ne.NextMove(st, mv))
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
            reached_states.push_back(std::make_pair(mv, st1));
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
    
    template <class Input, class Output, class State, class Move, class CostStructure>
    size_t MoveTester<Input, Output, State, Move, CostStructure>::Modality() const
    {
      return ne.Modality();
    }
    
    template <class Input, class Output, class State, class Move, class CostStructure>
    void MoveTester<Input, Output, State, Move, CostStructure>::SetTolerance(double t)
    {
      tolerance = t;
    }
    
  }
}
