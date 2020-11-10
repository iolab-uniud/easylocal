#pragma once

#include <stdexcept>
#include <fstream>
#include <chrono>
#include <future>
#include <iomanip>

#include "helpers/solutionmanager.hh"
#include "runners/runner.hh"
#include "testers/componenttester.hh"
#include "utils/types.hh"

namespace EasyLocal
{
  
  namespace Debug
  {
    
    using namespace EasyLocal::Core;
    
    class ChoiceReader
    {
    protected:
      /** Reads a choice from an input stream and converts it to an integer (-1 if it was not correct) */
      int ReadChoice(std::istream &is)
      {
        std::string c;
        std::cin >> c;
        try
        {
#ifdef HAVE_STD_STOI
          return std::stoi(c);
#else
          int value;
          std::istringstream iss(c);
          iss >> value;
          if (is.fail())
            throw std::invalid_argument("Not an integer value");
          return value;
#endif
        }
        catch (std::invalid_argument&)
        {
          return -1;
        }
      }
    };
    
    template <class Input, class Solution, class CostStructure>
    class AbstractTester
    {
    public:
      virtual ~AbstractTester(){};
      
    protected:
      virtual void AddRunner(Core::Runner<Input, Solution, CostStructure> &r){};
      void AddRunners()
      {
        for (auto p_r : Core::Runner<Input, Solution, CostStructure>::runners)
          AddRunner(*p_r);
      }
    };
    
    /** A Tester collects a set of basic testers (move, state, ...) and
     allows to access them through sub-menus. It represent the external
     user interface provided by the framework.
     @ingroup Testers
     */
    template <class Input, class Solution, class CostStructure = DefaultCostStructure<int>>
    class Tester : public AbstractTester<Input, Solution, CostStructure>, public ChoiceReader
    {
      typedef typename CostStructure::CFtype CFtype;
      
    public:
      Tester(const Input &in, SolutionManager<Input, Solution, CostStructure> &sm, std::ostream &os = std::cout);
      /** Virtual destructor. */
      virtual ~Tester() {}
      void RunMainMenu(std::string file_name = "");
      void AddMoveTester(ComponentTester<Input, Solution, CostStructure> &amt);
      void AddKickerTester(ComponentTester<Input, Solution, CostStructure> &kt);
      void RunInputMenu();
      void RunStateTestMenu();
      void SetState(const Solution &st) { test_state = st; }
      
    protected:
      void AddRunner(Core::Runner<Input, Solution, CostStructure> &r);
      void ShowStateMenu();
      void ShowReducedStateMenu();
      bool ExecuteStateChoice();
      void ShowMainMenu();
      void ShowMovesMenu();
      void ShowKickersMenu();
      void ShowRunMenu();
      void ShowSolverMenu();
      void ShowDebuggingMenu();
      void ExecuteMainChoice();
      void ExecuteMovesChoice();
      void ExecuteKickersChoice();
      void ExecuteRunChoice();
      void ExecuteSolverChoice();
      void ExecuteDebuggingMenu();
      std::vector<ComponentTester<Input, Solution, CostStructure> *> move_testers;
      std::vector<ComponentTester<Input, Solution, CostStructure> *> kicker_testers;
      /**< The set of attached move testers. */
      std::vector<Core::Runner<Input, Solution, CostStructure> *> runners; /**< The set of attached
                                                                         runners. */
      const Input &in;
      std::ostream &os;
      Core::SolutionManager<Input, Solution, CostStructure> &sm; /**< A pointer to a state manager. */
      Solution test_state;                                    /**< The current state managed by the tester. */
      int choice,                                          /**< The option currently chosen from the menu. */
      sub_choice;                                      /** The suboption currently chosen from the menu. */
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    /**
     Constructs a tester by providing it links to
     a state manager, an output manager, and an input object.
     
     @param sm a pointer to a compatible state manager
     @param om a pointer to a compatible output manager
     @param in a pointer to an input object
     */
    template <class Input, class Solution, class CostStructure>
    Tester<Input, Solution, CostStructure>::Tester(const Input &in,
                                                        Core::SolutionManager<Input, Solution, CostStructure> &sm,
std::ostream &os)
    : in(in), os(os), sm(sm), test_state(in)
    {
      this->AddRunners();
    }
    
    /**
     Adds a move tester.
     
     @param p_amt a pointer to a move tester
     */
    template <class Input, class Solution, class CostStructure>
    void Tester<Input, Solution, CostStructure>::AddMoveTester(ComponentTester<Input, Solution, CostStructure> &amt)
    {
      move_testers.push_back(&amt);
    }
    
    /**
     Adds a kicker tester.
     
     @param p_amt a pointer to a move tester
     */
    template <class Input, class Solution, class CostStructure>
    void Tester<Input, Solution, CostStructure>::AddKickerTester(ComponentTester<Input, Solution, CostStructure> &kt)
    {
      kicker_testers.push_back(&kt);
    }
    
    /**
     Adds a runner to the tester.
     
     @param p_ru a pointer to a compatible runner
     */
    template <class Input, class Solution, class CostStructure>
    void Tester<Input, Solution, CostStructure>::AddRunner(Core::Runner<Input, Solution, CostStructure> &r)
    {
      runners.push_back(&r);
    }
    
    /**
     Manages the tester main menu.
     */
    
    template <class Input, class Solution, class CostStructure>
    void Tester<Input, Solution, CostStructure>::RunMainMenu(std::string file_name)
    {
      if (file_name == "")
      {
        RunInputMenu();
      }
      else if (file_name == "random")
      {
        this->sm.RandomState(test_state);
      }
      else
      {
        std::ifstream is(file_name.c_str());
        if (is.fail())
          throw std::runtime_error("Cannot open file!");
        auto start = std::chrono::high_resolution_clock::now();
          is >> test_state;
        os << "SOLUTION IMPORTED " << std::endl
        << test_state << std::endl;
        os << "IMPORTED SOLUTION COST : " << sm.CostFunctionComponents(test_state) << std::endl;
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = end - start;
        os << "ELAPSED TIME : " << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() / 1000.0 << " s" << std::endl;
      }
      
      do
      {
        ShowMainMenu();
        if (this->choice != 0)
          ExecuteMainChoice();
      } while (this->choice != 0);
      os << "Bye bye..." << std::endl;
    }
    
    /**
     Outputs the main menu options.
     */
    template <class Input, class Solution, class CostStructure>
    void Tester<Input, Solution, CostStructure>::ShowMainMenu()
    {
      os << "MAIN MENU:" << std::endl
      << "   (1) Move menu" << std::endl
      << "   (2) Kicker menu" << std::endl
      << "   (3) Run menu" << std::endl
      << "   (4) State menu" << std::endl
      << "   (0) Exit" << std::endl
      << " Your choice: ";
      this->choice = this->ReadChoice(std::cin);
    }
    
    /**
     Execute a choice made in the main menu.
     */
    template <class Input, class Solution, class CostStructure>
    void Tester<Input, Solution, CostStructure>::ExecuteMainChoice()
    {
      switch (this->choice)
      {
        case 1:
          ShowMovesMenu();
          ExecuteMovesChoice();
          break;
        case 2:
          ShowKickersMenu();
          ExecuteKickersChoice();
          break;
        case 3:
          ShowRunMenu();
          ExecuteRunChoice();
          break;
        case 4:
          RunStateTestMenu();
          break;
        case 0:
          break;
        default:
          os << "Invalid choice" << std::endl;
      }
    }
    
    /**
     Outputs the menu for the move testers.
     */
    template <class Input, class Solution, class CostStructure>
    void Tester<Input, Solution, CostStructure>::ShowMovesMenu()
    {
      unsigned int i;
      os << "MOVE MENU: " << std::endl;
      for (i = 0; i < move_testers.size(); i++)
        os << "   (" << i + 1 << ") " << move_testers[i]->name << " [" << move_testers[i]->Modality() << "-modal]" << std::endl;
      os << "   (0) Return to Main Menu" << std::endl;
      os << " Your choice: ";
      this->sub_choice = this->ReadChoice(std::cin);
    }
    
    template <class Input, class Solution, class CostStructure>
    void Tester<Input, Solution, CostStructure>::ShowSolverMenu()
    {
      os << "SOLVER MENU: " << std::endl;
      os << "   (1) Simple solver" << std::endl;
      os << "   (2) Token ring solver" << std::endl;
      os << "   (0) Return to Main Menu" << std::endl;
      os << " Your choice: ";
      this->sub_choice = this->ReadChoice(std::cin);
    }
    
    /**
     Outputs the menu for the move testers.
     */
    template <class Input, class Solution, class CostStructure>
    void Tester<Input, Solution, CostStructure>::ShowKickersMenu()
    {
      unsigned int i;
      os << "KICK MENU: " << std::endl;
      for (i = 0; i < kicker_testers.size(); i++)
        os << "   (" << i + 1 << ") " << kicker_testers[i]->name << std::endl;
      os << "   (0) Return to Main Menu" << std::endl;
      os << " Your choice: ";
      this->sub_choice = this->ReadChoice(std::cin);
    }
    
    /**
     Outputs the menu for the runners.
     */
    template <class Input, class Solution, class CostStructure>
    void Tester<Input, Solution, CostStructure>::ShowRunMenu()
    {
      unsigned int i;
      do
      {
        os << "RUN MENU: " << std::endl;
        for (i = 0; i < runners.size(); i++)
          os << "   (" << (i + 1) << ") " << runners[i]->name << std::endl;
        os << "   (0) Return to Main Menu" << std::endl;
        os << " Your choice: ";
        this->sub_choice = this->ReadChoice(std::cin);
        if (sub_choice == -1 || sub_choice >= static_cast<int>(runners.size()))
          os << "Invalid choice" << std::endl;
      } while (sub_choice == -1 || sub_choice > static_cast<int>(runners.size()));
    }
    
    /**
     Execute a choice from the move testers menu.
     */
    template <class Input, class Solution, class CostStructure>
    void Tester<Input, Solution, CostStructure>::ExecuteMovesChoice()
    {
      if (sub_choice > 0 && sub_choice <= static_cast<int>(move_testers.size()))
        move_testers[sub_choice - 1]->RunMainMenu(test_state);
    }
    
    /**
     Execute a choice from the move testers menu.
     */
    template <class Input, class Solution, class CostStructure>
    void Tester<Input, Solution, CostStructure>::ExecuteKickersChoice()
    {
      if (sub_choice > 0)
        kicker_testers[sub_choice - 1]->RunMainMenu(test_state);
    }
    
    /**
     Execute a choice from the runners menu.
     */
    template <class Input, class Solution, class CostStructure>
    void Tester<Input, Solution, CostStructure>::ExecuteRunChoice()
    {
      if (sub_choice > 0)
      {
        Runner<Input, Solution, CostStructure> &r = *runners[sub_choice - 1];
        r.ReadParameters();
        
        // Ask for timeout
        double timeout;
        os << "  Timeout: ";
        std::cin >> timeout;
        os << std::endl;
        auto to = std::chrono::milliseconds((long long)(timeout * 1000));
        
        auto start = std::chrono::high_resolution_clock::now();
        CostStructure result = r.SyncRun(to, this->test_state);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = end - start;
        
          // FIXME: print solution in a nice format
        os << "CURRENT SOLUTION " << std::endl
           << test_state << std::endl;
        os << "CURRENT COST : " << sm.CostFunctionComponents(this->test_state) << std::endl;
        os << "ELAPSED TIME : " << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() / 1000.0 << " s" << std::endl;
        os << "NUMBER OF ITERATIONS : " << r.Iteration() << std::endl;
      }
    }
    
    /**
     Manages an adbridged menu for building the initial state.
     */
    template <class Input, class Solution, class CostStructure>
    void Tester<Input, Solution, CostStructure>::RunInputMenu()
    {
      bool show_state;
      ShowReducedStateMenu();
      std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
      show_state = ExecuteStateChoice();
      std::chrono::milliseconds duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start);
      if (show_state)
      {
        os << "INITIAL SOLUTION " << std::endl
        << test_state << std::endl;
        os << "INITIAL COST : " << this->sm.CostFunctionComponents(test_state) << std::endl;
      }
      os << "ELAPSED TIME : " << duration.count() / 1000.0 << "s" << std::endl;
    }
    
    /**
     Outputs the menu options.
     */
    template <class Input, class Solution, class CostStructure>
    void Tester<Input, Solution, CostStructure>::ShowStateMenu()
    {
      os << "STATE MENU: " << std::endl
      << "    (1) Random state " << std::endl
      << "    (2) Read from file" << std::endl
      << "    (3) Greedy state " << std::endl
      << "    (4) Sample state" << std::endl
      << "    (5) Write to file" << std::endl
      << "    (6) Show state" << std::endl
      << "    (7) Show input" << std::endl
      << "    (8) Show cost function components" << std::endl
      << "    (9) Show cost elements" << std::endl
      << "    (10) Check state consistency" << std::endl
      << "    (11) Pretty print output" << std::endl
      << "    (0) Return to Main Menu" << std::endl
      << "Your choice : ";
      this->sub_choice = this->ReadChoice(std::cin);
    }
    
    /**
     Outputs a reduced set of options for the initial state building.
     */
    template <class Input, class Solution, class CostStructure>
    void Tester<Input, Solution, CostStructure>::ShowReducedStateMenu()
    {
      os << "INITIAL STATE MENU: " << std::endl
      << "    (1) Random state " << std::endl
      << "    (2) Read from file" << std::endl
      << "    (3) Greedy state " << std::endl
      << "Your choice : ";
      this->sub_choice = this->ReadChoice(std::cin);
      if (sub_choice >= 4)
        sub_choice = -1;
    }
    
    /**
     Execute the menu choice on the given state.
     
     @param st the current state
     */
    template <class Input, class Solution, class CostStructure>
    bool Tester<Input, Solution, CostStructure>::ExecuteStateChoice()
    {
      unsigned int i;
      std::string file_name;
      switch (sub_choice)
      {
        case 1:
          this->sm.RandomState(test_state);
          break;
        case 2:
        {
          bool read_failed;
          std::ifstream is;
          do
          {
            read_failed = false;
            os << "File name : ";
            std::cin >> file_name;
            is.open(file_name.c_str());
            if (is.fail())
            {
              os << "File " << file_name << " does not exist!" << std::endl;
              read_failed = true;
              is.clear();
            }
          } while (read_failed);
            is >> test_state;
          break;
        }
        case 3:
        {
          //           unsigned int lenght;
          //           double randomness;
          //           os << "Lenght of the restricted candidate list: ";
          //           std::cin >> lenght;
          //           os << "Level of randomness (0 <= alpha <= 1): ";
          //           std::cin >> randomness;
          //           this->sm.GreedyState(test_state, randomness, lenght);
          this->sm.GreedyState(test_state);
          break;
        }
        case 4:
        {
          unsigned int samples;
          os << "How many samples : ";
          std::cin >> samples;
          this->sm.SampleState(test_state, samples);
          break;
        }
        case 5:
        {
          os << "File name : ";
          std::cin >> file_name;
            // FIXME: print it better
          std::ofstream os(file_name.c_str());
            os << test_state;
          break;
        }
        case 6:
        {
          os << test_state << std::endl;
          os << "Total cost: " << this->sm.CostFunctionComponents(test_state) << std::endl;
          break;
        }
        case 7:
        {
          os << in;
          break;
        }
        case 8:
        {
          os << "Cost Components: " << std::endl;
          CostStructure cost = this->sm.CostFunctionComponents(test_state);
          for (i = 0; i < sm.CostComponents(); i++)
          {
            const CostComponent<Input, Solution, typename CostStructure::CFtype> &cc = sm.GetCostComponent(i);
            os << i << ". " << cc.name << " : "
            << cost.all_components[i] << (cc.IsHard() ? '*' : ' ') << std::endl;
          }
          os << "Total Violations: " << cost.violations << std::endl;
          os << "Total Objective:  " << cost.objective << std::endl;
          os << "Total Cost:       " << cost.total << std::endl;
          break;
        }
        case 9:
        {
          os << "Detailed Violations: " << std::endl;
          CostStructure cost = this->sm.CostFunctionComponents(test_state);
          for (i = 0; i < sm.CostComponents(); i++)
          {
            const CostComponent<Input, Solution, typename CostStructure::CFtype> &cc = sm.GetCostComponent(i);
            cc.PrintViolations(test_state);
          }
          os << std::endl
          << "Summary of Cost Components: " << std::endl;
          for (i = 0; i < sm.CostComponents(); i++)
          {
            const CostComponent<Input, Solution, typename CostStructure::CFtype> &cc = sm.GetCostComponent(i);
            os << i << ". " << cc.name << " : "
            << cost.all_components[i] << (cc.IsHard() ? '*' : ' ') << std::endl;
          }
          os << "Total Violations: " << cost.violations << std::endl;
          os << "Total Objective:  " << cost.objective << std::endl;
          os << "Total Cost:       " << cost.total << std::endl;
          break;
        }
        case 10:
        {
          os << "Checking state consistency: " << std::endl;
          bool consistent = this->sm.CheckConsistency(test_state);
          if (consistent)
            os << "The state is consistent" << std::endl;
          else
            os << "The state is not consistent" << std::endl;
          break;
        }
        case 11:
        {
          os << "File name : ";
          std::cin >> file_name;
            this->sm.PrettyPrintOutput(test_state, file_name);
          std::cout << "Output pretty-printed in file " << file_name << std::endl;
          break;
        }
        default:
          os << "Invalid choice" << std::endl;
      }
      return (sub_choice >= 1 && sub_choice <= 4);
    }
    
    /**
     Manages the component tester menu for the given state.
     @param st the state to test
     */
    template <class Input, class Solution, class CostStructure>
    void Tester<Input, Solution, CostStructure>::RunStateTestMenu()
    {
      bool show_state;
      do
      {
        ShowStateMenu();
        if (sub_choice != 0)
        {
          std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
          show_state = ExecuteStateChoice();
          std::chrono::milliseconds duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start);
          if (show_state)
          {
              // FIXME: print solution in a nice format
            os << "CURRENT SOLUTION " << std::endl
            << test_state << std::endl;
            os << "CURRENT COST : " << sm.CostFunctionComponents(test_state) << std::endl;
          }
          os << "ELAPSED TIME : " << duration.count() / 1000.0 << "s" << std::endl;
        }
      } while (sub_choice != 0);
      os << "Leaving state menu" << std::endl;
    }
  } // namespace Debug
} // namespace EasyLocal
