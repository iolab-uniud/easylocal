#pragma once

#include <stdexcept>
#include <fstream>
#include <chrono>
#include <future>
#include <iomanip>

#include "helpers/statemanager.hh"
#include "runners/runner.hh"
#include "testers/componenttester.hh"
#include "utils/types.hh"

namespace EasyLocal
{
  
  namespace Debug
  {
    
    using namespace EasyLocal::Core;
    
    template <class NeighborhoodExplorer>
    class MoveTester;
    
    template <class Kicker>
    class KickerTester;
    
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
        catch (std::invalid_argument)
        {
          return -1;
        }
      }
    };
    
    template <class Input, class State, class CostStructure>
    class AbstractTester
    {
    public:
      AbstractTester() : p_in(nullptr), p_test_state(nullptr)
      {
        this->AddRunners();
      }
      AbstractTester(const Input& in) : p_in(&in)
      {
        p_test_state = std::make_shared<State>(*p_in);
        this->AddRunners();
      }
      void AddRunner(Core::Runner<Input, State, CostStructure> &r);
      virtual ~AbstractTester(){};
    protected:
      void AddRunners()
      {
        for (auto p_r : Core::Runner<Input, State, CostStructure>::runners)
          AddRunner(*p_r);
      }
      
      virtual void SetInput(const Input& in)
      {
        p_in = &in;
        p_test_state = std::make_shared<State>(*p_in);
      }
      
      const Input& GetInput() const
      {
        if (!p_in)
          throw std::runtime_error("Error: input object not passed to tester yet");
        return *p_in;
      }
      
      State& GetTestState() const
      {
        if (!p_test_state)
          throw std::runtime_error("Error: state object was not set in tester yet");
        return *p_test_state;
      }
      
      void SetTestState(const State &st)
      {
        // FIXME: here the input of the passed state should be tested if it matches with the current p_in
        this->GetTestState() = std::make_shared<State>(st);
      }
      
      const Input* p_in; /**< The current input managed by the tester. */
      std::shared_ptr<State> p_test_state; /**< The current state managed by the tester. */
      std::vector<Core::Runner<Input, State, CostStructure> *> runners; /**< The set of attached runners. */
    };
    
    /**
     Adds a runner to the tester.
     
     @param p_ru a pointer to a compatible runner
     */
    template <class Input, class State, class CostStructure>
    void AbstractTester<Input, State, CostStructure>::AddRunner(Core::Runner<Input, State, CostStructure> &r)
    {
      runners.push_back(&r);
    }
    
    /** A Tester collects a set of basic testers (move, state, ...) and
     allows to access them through sub-menus. It represent the external
     user interface provided by the framework.
     @ingroup Testers
     */
    template <class Input, class State, class CostStructure>
    class Tester : public AbstractTester<Input, State, CostStructure>, public ChoiceReader
    {
      typedef typename CostStructure::CFtype CFtype;
    public:
      Tester(const Input &in, StateManager<Input, State, CostStructure> &sm, std::ostream &o = std::cout);
      // constructor
      Tester(StateManager<Input, State, CostStructure> &sm,
             std::ostream &o = std::cout);
      
      /** Virtual destructor. */
      virtual ~Tester()
      {
        for (auto ct : created_testers)
          delete ct;
      }
      
      void RunMainMenu(std::string file_name="");
      void RunMainMenu(const Input& in, std::string file_name="")
      {
        AbstractTester<Input, State, CostStructure>::SetInput(in);
        RunMainMenu(file_name);
      }
      
      template <class Kicker>
      void AddKickerTester(Kicker&k, std::string name)
      {
        auto kt = std::make_unique<KickerTester<Kicker>>(sm, k, name, os);
        kicker_testers.emplace_back(std::move(kt));
      }
      
      template <class NeighborhoodExplorer>
      void AddMoveTester(NeighborhoodExplorer&nhe, std::string name)
      {
        auto mt = std::make_unique<MoveTester<NeighborhoodExplorer>>(sm, nhe, name, os);
        move_testers.emplace_back(std::move(mt));
      }
      
      void SetInput(const Input& in)
      {
        AbstractTester<Input, State, CostStructure>::SetInput(in);
        RunInputMenu();
      }
    protected:
      void RunInputMenu();
      void RunStateTestMenu();
      void ShowStateMenu();
      void ShowReducedStateMenu();
      bool ExecuteStateChoice();
      void ShowMainMenu();
      void ShowMovesMenu();
      void ShowKickersMenu();
      void ShowRunMenu();
      void ShowSolverMenu();
      void ShowLoadInputMenu();
      void ShowDebuggingMenu();
      void ExecuteMainChoice();
      void ExecuteMovesChoice();
      void ExecuteKickersChoice();
      void ExecuteRunChoice();
      void ExecuteSolverChoice();
      void ExecuteDebuggingMenu();
      std::vector<std::unique_ptr<ComponentTester<Input, State, CostStructure>>> move_testers;
      std::vector<std::unique_ptr<ComponentTester<Input, State, CostStructure>>> kicker_testers;
      std::ostream &os;
      Core::StateManager<Input, State, CostStructure> &sm; /**< A pointer to a state manager. */
      int choice,                                      /**< The option currently chosen from the menu. */
      sub_choice;                                      /**< The suboption currently chosen from the menu. */
    private:
      std::list<ComponentTester<Input, State, CostStructure>*> created_testers;
      std::unique_ptr<Input> internal_input; /**< for creating an input internally to the tester. */
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    /**
     Constructs a tester by providing it links to
     a state manager, an output manager, and an input object.

     @param in an input object
     @param sm a compatible state manager
     @param om a compatible output manager
     */
    template <class Input, class State, class CostStructure>
    Tester<Input, State, CostStructure>::Tester(const Input &in,
                                                Core::StateManager<Input, State, CostStructure> &sm,
                                                std::ostream &os)
    : AbstractTester<Input, State, CostStructure>(in), os(os), sm(sm), internal_input(nullptr)
    {}
    
    /**
     Constructs a tester by providing it links to
     a state manager, an output manager, and an input object.
     
     @param in an input object
     @param sm a compatible state manager
     @param om a compatible output manager
     */
    template <class Input, class State, class CostStructure>
    Tester<Input, State, CostStructure>::Tester(Core::StateManager<Input, State, CostStructure> &sm, std::ostream &os)
    : os(os), sm(sm), internal_input(nullptr)
    {}
    
    
    /**
     Manages the tester main menu.
     */
    
    template <class Input, class State, class CostStructure>
    void Tester<Input, State, CostStructure>::RunMainMenu(std::string file_name)
    {
      if (file_name == "")
      {
        RunInputMenu();
      }
      else if (file_name == "random")
      {
        this->sm.RandomState(this->GetInput(), this->GetTestState());
      }
      else
      {
        std::ifstream is(file_name.c_str());
        if (is.fail())
          throw std::runtime_error("Cannot open file!");
        is >> this->GetTestState();
        os << "SOLUTION IMPORTED " << std::endl;
        os << this->GetTestState();
        os << "IMPORTED SOLUTION COST: " << sm.CostFunctionComponents(this->GetInput(), this->GetTestState()) << std::endl;
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
    template <class Input, class State, class CostStructure>
    void Tester<Input, State, CostStructure>::ShowMainMenu()
    {
      os << "MAIN MENU:" << std::endl
      << "   (1) Move menu" << std::endl
      << "   (2) Kicker menu" << std::endl
      << "   (3) Run menu" << std::endl
      << "   (4) State menu" << std::endl
      << "   (5) Load new input" << std::endl
      << "   (0) Exit" << std::endl
      << " Your choice: ";
      this->choice = this->ReadChoice(std::cin);
    }
    
    /**
     Execute a choice made in the main menu.
     */
    template <class Input, class State, class CostStructure>
    void Tester<Input, State, CostStructure>::ExecuteMainChoice()
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
        case 5:
          ShowLoadInputMenu();
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
    template <class Input, class State, class CostStructure>
    void Tester<Input, State, CostStructure>::ShowMovesMenu()
    {
      unsigned int i;
      os << "MOVE MENU: " << std::endl;
      for (i = 0; i < move_testers.size(); i++)
        os << "   (" << i + 1 << ") " << move_testers[i]->name << " [" << move_testers[i]->Modality() << "-modal]" << std::endl;
      os << "   (0) Return to Main Menu" << std::endl;
      os << " Your choice: ";
      this->sub_choice = this->ReadChoice(std::cin);
    }
    
    template <class Input, class State, class CostStructure>
    void Tester<Input, State, CostStructure>::ShowSolverMenu()
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
    template <class Input, class State, class CostStructure>
    void Tester<Input, State, CostStructure>::ShowKickersMenu()
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
    template <class Input, class State, class CostStructure>
    void Tester<Input, State, CostStructure>::ShowRunMenu()
    {
      unsigned int i;
      do
      {
        os << "RUN MENU: " << std::endl;
        for (i = 0; i < this->runners.size(); i++)
          os << "   (" << (i + 1) << ") " << this->runners[i]->name << std::endl;
        os << "   (0) Return to Main Menu" << std::endl;
        os << " Your choice: ";
        this->sub_choice = this->ReadChoice(std::cin);
        if (sub_choice == -1 || sub_choice >= static_cast<int>(this->runners.size()))
          os << "Invalid choice" << std::endl;
      } while (sub_choice == -1 || sub_choice > static_cast<int>(this->runners.size()));
    }
    
    /**
     Execute a choice from the move testers menu.
     */
    template <class Input, class State, class CostStructure>
    void Tester<Input, State, CostStructure>::ExecuteMovesChoice()
    {
      if (sub_choice > 0 && sub_choice <= static_cast<int>(move_testers.size()))
        move_testers[sub_choice - 1]->RunMainMenu(this->GetInput(), this->GetTestState());
    }
    
    /**
     Execute a choice from the move testers menu.
     */
    template <class Input, class State, class CostStructure>
    void Tester<Input, State, CostStructure>::ExecuteKickersChoice()
    {
      if (sub_choice > 0 && sub_choice <= static_cast<int>(kicker_testers.size()))
        kicker_testers[sub_choice - 1]->RunMainMenu(this->GetInput(), this->GetTestState());
    }
    
    /**
     Execute a choice from the runners menu.
     */
    template <class Input, class State, class CostStructure>
    void Tester<Input, State, CostStructure>::ExecuteRunChoice()
    {
      if (sub_choice > 0)
      {
        Runner<Input, State, CostStructure> &r = *this->runners[sub_choice - 1];
        r.ReadParameters();
        
        // Ask for timeout
        double timeout;
        os << "  Timeout: ";
        std::cin >> timeout;
        os << std::endl;
        auto to = std::chrono::milliseconds((long long)(timeout * 1000));
        
        auto start = std::chrono::high_resolution_clock::now();
        CostStructure result = r.SyncRun(to, this->GetInput(), this->GetTestState());
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = end - start;
        os << "CURRENT SOLUTION " << std::endl;
        os << this->GetTestState();
        os << "CURRENT COST: " << result << std::endl;
        os << "ELAPSED TIME: " << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() / 1000.0 << " s" << std::endl;
        os << "NUMBER OF ITERATIONS: " << r.Iteration() << std::endl;
      }
    }
    
    /**
     Manages an adbridged menu for building the initial state.
     */
    template <class Input, class State, class CostStructure>
    void Tester<Input, State, CostStructure>::RunInputMenu()
    {
      bool show_state;
      ShowReducedStateMenu();
      std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
      show_state = ExecuteStateChoice();
      std::chrono::milliseconds duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start);
      if (show_state)
      {
        os << "INITIAL SOLUTION " << std::endl;
        os << this->GetTestState();
        os << "INITIAL COST: " << this->sm.CostFunctionComponents(this->GetInput(), this->GetTestState()) << std::endl;
      }
      os << "ELAPSED TIME: " << duration.count() / 1000.0 << "s" << std::endl;
    }
    
    /**
     Outputs the menu options.
     */
    template <class Input, class State, class CostStructure>
    void Tester<Input, State, CostStructure>::ShowStateMenu()
    {
      os << "STATE MENU: " << std::endl
      << "    (1) Random state " << std::endl
      << "    (2) Read from file" << std::endl
      << "    (3) Greedy state " << std::endl
      << "    (4) Sample state" << std::endl
      << "    (5) Write to file" << std::endl
      << "    (6) Show detailed state" << std::endl
      << "    (7) Show input" << std::endl
      << "    (8) Show cost function components" << std::endl
      << "    (9) Show cost elements" << std::endl
      << "    (10) Check state consistency" << std::endl
      << "    (11) Write JSON state to file" << std::endl
      << "    (0) Return to Main Menu" << std::endl
      << "Your choice: ";
      this->sub_choice = this->ReadChoice(std::cin);
    }
    
    /**
     Outputs a reduced set of options for the initial state building.
     */
    template <class Input, class State, class CostStructure>
    void Tester<Input, State, CostStructure>::ShowReducedStateMenu()
    {
      os << "INITIAL STATE MENU: " << std::endl
      << "    (1) Random state " << std::endl
      << "    (2) Read from file" << std::endl
      << "    (3) Greedy state " << std::endl
      << "Your choice: ";
      this->sub_choice = this->ReadChoice(std::cin);
      if (sub_choice >= 4)
        sub_choice = -1;
    }
    
    // FIXME: currently it relies on the fact that an Input constructor with a single string parameter exists
    template <class Input, class State, class CostStructure>
    void Tester<Input, State, CostStructure>::ShowLoadInputMenu()
    {
      std::string file_name = "";
      do
      {
        os << "NEW INPUT" << std::endl;
        os << "(write exit to go to the previous menu)" << std::endl;
        os << "Enter the filename or the value (no spaces allowed): ";
        std::cin >> file_name;
        if (file_name == "exit")
          break;
        if (file_name == "")
          os << "You should provide a valid filename" << std::endl;
        else
        {
          try
          {
            // this will destroy a previously created object, if exists
            internal_input = std::make_unique<Input>(file_name);
            SetInput(*internal_input);
            break;
          }
          catch (std::exception e)
          {
            os << "Something went wrong " << e.what() << std::endl;
            file_name = "";
          }
        }
      }
      while (file_name != "");
    }
    
    /**
     Execute the menu choice on the given state.
     
     @param st the current state
     */
    template <class Input, class State, class CostStructure>
    bool Tester<Input, State, CostStructure>::ExecuteStateChoice()
    {
      unsigned int i;
      std::string file_name;
      switch (sub_choice)
      {
        case 1:
          this->sm.RandomState(this->GetInput(), this->GetTestState());
          break;
        case 2:
        {
          bool read_failed;
          std::ifstream is;
          do
          {
            read_failed = false;
            os << "File name: ";
            std::cin >> file_name;
            is.open(file_name.c_str());
            if (is.fail())
            {
              os << "File " << file_name << " does not exist!" << std::endl;
              read_failed = true;
              is.clear();
            }
          } while (read_failed);
          is >> this->GetTestState();
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
          //           this->sm.GreedyState(this->GetTestState(), randomness, lenght);
          this->sm.GreedyState(this->GetInput(), this->GetTestState());
          break;
        }
        case 4:
        {
          unsigned int samples;
          os << "How many samples: ";
          std::cin >> samples;
          this->sm.SampleState(this->GetInput(), this->GetTestState(), samples);
          break;
        }
        case 5:
        {
          os << "File name: ";
          std::cin >> file_name;
          std::ofstream os(file_name.c_str());
          os << this->GetTestState();
          break;
        }
        case 6:
        {
          os << this->GetTestState();
          os << "Total cost: " << this->sm.CostFunctionComponents(this->GetInput(), this->GetTestState()) << std::endl;
          break;
        }
        case 7:
        {
          os << this->GetInput();
          break;
        }
        case 8:
        {
          os << "Cost Components: " << std::endl;
          CostStructure cost = this->sm.CostFunctionComponents(this->GetInput(), this->GetTestState());
          for (i = 0; i < sm.CostComponents(); i++)
          {
            const CostComponent<Input, State, typename CostStructure::CFtype> &cc = sm.GetCostComponent(i);
            os << i << ". " << cc.name << ": "
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
          CostStructure cost = this->sm.CostFunctionComponents(this->GetInput(), this->GetTestState());
          for (i = 0; i < sm.CostComponents(); i++)
          {
            const CostComponent<Input, State, typename CostStructure::CFtype> &cc = sm.GetCostComponent(i);
            cc.PrintViolations(this->GetInput(), this->GetTestState());
          }
          os << std::endl
          << "Summary of Cost Components: " << std::endl;
          for (i = 0; i < sm.CostComponents(); i++)
          {
            const CostComponent<Input, State, typename CostStructure::CFtype> &cc = sm.GetCostComponent(i);
            os << i << ". " << cc.name << ": "
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
          bool consistent = this->sm.CheckConsistency(this->GetInput(), this->GetTestState());
          if (consistent)
            os << "The state is consistent" << std::endl;
          else
            os << "The state is not consistent" << std::endl;
          break;
        }
        case 11:
        {
          os << "File name: ";
          std::cin >> file_name;
          std::ofstream os(file_name.c_str());
          os << this->sm.ToJSON(this->GetInput(), this->GetTestState());
          std::cout << "Output written as JSON in file " << file_name << std::endl;
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
    template <class Input, class State, class CostStructure>
    void Tester<Input, State, CostStructure>::RunStateTestMenu()
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
            os << "CURRENT SOLUTION " << std::endl;
            os << this->GetTestState();
            os << "CURRENT COST: " << sm.CostFunctionComponents(this->GetInput(), this->GetTestState()) << std::endl;
          }
          os << "ELAPSED TIME: " << duration.count() / 1000.0 << "s" << std::endl;
        }
      } while (sub_choice != 0);
      os << "Leaving state menu" << std::endl;
    }
  } // namespace Debug
} // namespace EasyLocal
