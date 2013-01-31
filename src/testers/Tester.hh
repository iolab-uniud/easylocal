#if !defined(_TESTER_HH_)
#define _TESTER_HH_

#include <helpers/StateManager.hh>
#include <helpers/OutputManager.hh>
#include <runners/Runner.hh>
#include <testers/ComponentTester.hh>
#include <stdexcept>
#include <fstream>
#include <utils/Types.hh>
#include <chrono>
#include <future>
#include <iomanip>

template <class Input, class State, typename CFtype>
class AbstractTester
{
public:
  virtual ~AbstractTester() {};
protected:
  virtual void AddRunner(Runner<Input,State,CFtype>& r) {};
  void AddRunners()
  {
    for (auto p_r : Runner<Input,State,CFtype>::runners)
      AddRunner(*p_r);
  }
};

/** A Tester collects a set of basic testers (move, state, ...) and
 allows to access them through sub-menus. It represent the external
 user interface provided by the framework.
 @ingroup Testers
 */
template <class Input, class Output, class State, typename CFtype>
class Tester : public AbstractTester<Input, State, CFtype>
{
public:
  Tester(const Input& in, StateManager<Input,State,CFtype>& e_sm,
         OutputManager<Input,Output,State,CFtype>& e_om, std::ostream& o = std::cout);
  Tester(const Input& in, State st, StateManager<Input,State,CFtype>& e_sm,
         OutputManager<Input,Output,State,CFtype>& e_om, std::ostream& o = std::cout);
  /** Virtual destructor. */
  virtual ~Tester() {}
  void RunMainMenu(std::string file_name = "");
  void AddMoveTester(ComponentTester<Input,Output,State,CFtype>& amt);
  void AddKickerTester(ComponentTester<Input,Output,State,CFtype>& kt);
  void RunInputMenu();
  void RunStateTestMenu();
  void SetState(const State& st){test_state = st;}
protected:
  void AddRunner(Runner<Input,State,CFtype>& r);
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
  std::vector<ComponentTester<Input,Output,State,CFtype>* > move_testers;
  std::vector<ComponentTester<Input,Output,State,CFtype>* > kicker_testers;
  /**< The set of attached move testers. */
  std::vector<Runner<Input,State,CFtype>* > runners; /**< The set of attached
                                                      runners. */
  const Input& in;
  std::ostream& os;
  StateManager<Input,State,CFtype>& sm;  /**< A pointer to a state manager. */
  OutputManager<Input,Output,State,CFtype>& om; /**< A pointer to an output producer. */
  State test_state; /**< The current state managed by the tester. */
  Output out; /**< The output object. */
  unsigned int choice, /**< The option currently chosen from the menu. */
  sub_choice; /** The suboption currently chosen from the menu. */
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
template <class Input, class Output, class State, typename CFtype>
Tester<Input, Output, State, CFtype>::Tester(const Input& i,
                                             StateManager<Input,State,CFtype>& e_sm,
                                             OutputManager<Input,Output,State,CFtype>& e_om, std::ostream& o)
:  in(i), os(o), sm(e_sm), om(e_om),
test_state(i), out(i)
{ this->AddRunners(); }


/**
 Adds a move tester.
 
 @param p_amt a pointer to a move tester
 */
template <class Input, class Output, class State, typename CFtype>
void Tester<Input, Output, State,CFtype>::AddMoveTester(ComponentTester<Input,Output,State,CFtype>& amt)
{ move_testers.push_back(&amt); }

/**
 Adds a kicker tester.
 
 @param p_amt a pointer to a move tester
 */
template <class Input, class Output, class State, typename CFtype>
void Tester<Input, Output, State,CFtype>::AddKickerTester(ComponentTester<Input,Output,State,CFtype>& kt)
{ kicker_testers.push_back(&kt); }

/**
 Adds a runner to the tester.
 
 @param p_ru a pointer to a compatible runner
 */
template <class Input, class Output, class State, typename CFtype>
void Tester<Input, Output, State,CFtype>::AddRunner(Runner<Input,State,CFtype>& r)
{
  runners.push_back(&r);
}

/**
 Manages the tester main menu.
 */

template <class Input, class Output, class State, typename CFtype>
void Tester<Input, Output, State,CFtype>::RunMainMenu(std::string file_name)
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
    om.ReadState(test_state, is);
    om.OutputState(test_state, out);
    os << "SOLUTION IMPORTED " << std::endl << out << std::endl;
    os << "IMPORTED SOLUTION COST : " << sm.CostFunction(test_state) << std::endl;
  }
  
  do
  {
    ShowMainMenu();
    if (this->choice != 0)
      ExecuteMainChoice();
  }
  while (this->choice != 0);
  os << "Bye bye..." << std::endl;
}

/**
 Outputs the main menu options.
 */
template <class Input, class Output, class State, typename CFtype>
void Tester<Input,Output, State,CFtype>::ShowMainMenu()
{
  os << "MAIN MENU:" << std::endl
  << "   (1) Move menu" << std::endl
  << "   (2) Kicker menu" << std::endl
  << "   (3) Run menu" << std::endl
  << "   (4) State menu" << std::endl
  << "   (0) Exit" << std::endl
  << " Your choice: ";
  std::cin >> this->choice;
}

/**
 Execute a choice made in the main menu.
 */
template <class Input, class Output, class State, typename CFtype>
void Tester<Input, Output,State,CFtype>::ExecuteMainChoice()
{
  switch(this->choice)
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
template <class Input, class Output, class State, typename CFtype>
void Tester<Input, Output,State,CFtype>::ShowMovesMenu()
{
  unsigned int i;
  os << "MOVE MENU: " << std::endl;
  for (i = 0; i < move_testers.size(); i++)
    os << "   (" << i+1 << ") " << move_testers[i]->name << " [" << move_testers[i]->Modality() << "-modal]" << std::endl;
  os << "   (0) Return to Main Menu" << std::endl;
  os << " Your choice: ";
  std::cin >> sub_choice;
}


template <class Input, class Output, class State, typename CFtype>
void Tester<Input, Output,State,CFtype>::ShowSolverMenu()
{
  os << "SOLVER MENU: " << std::endl;
  os << "   (1) Simple solver" << std::endl;
  os << "   (2) Token ring solver" << std::endl;
  os << "   (0) Return to Main Menu" << std::endl;
  os << " Your choice: ";
  std::cin >> sub_choice;
}

/**
 Outputs the menu for the move testers.
 */
template <class Input, class Output, class State, typename CFtype>
void Tester<Input, Output,State,CFtype>::ShowKickersMenu()
{
  unsigned int i;
  os << "KICK MENU: " << std::endl;
  for (i = 0; i < kicker_testers.size(); i++)
    os << "   (" << i+1 << ") " << kicker_testers[i]->name << std::endl;
  os << "   (0) Return to Main Menu" << std::endl;
  os << " Your choice: ";
  std::cin >> sub_choice;
}

/**
 Outputs the menu for the runners.
 */
template <class Input, class Output, class State, typename CFtype>
void Tester<Input,Output, State,CFtype>::ShowRunMenu()
{
  unsigned int i;
  os << "RUN MENU: " << std::endl;
  for (i = 0; i < runners.size(); i++)
    os << "   (" << (i + 1) << ") " << runners[i]->name << std::endl;
  os << "   (0) Return to Main Menu" << std::endl;
  os << " Your choice: ";
  std::cin >> sub_choice;
}

/**
 Execute a choice from the move testers menu.
 */
template <class Input, class Output, class State, typename CFtype>
void Tester<Input,Output, State,CFtype>::ExecuteMovesChoice()
{
  if (sub_choice > 0 && sub_choice <= move_testers.size())
    move_testers[sub_choice-1]->RunMainMenu(test_state);
}

/**
 Execute a choice from the move testers menu.
 */
template <class Input, class Output, class State, typename CFtype>
void Tester<Input,Output, State,CFtype>::ExecuteKickersChoice()
{
  if (sub_choice > 0)
    kicker_testers[sub_choice-1]->RunMainMenu(test_state);
}

/**
 Execute a choice from the runners menu.
 */
template <class Input, class Output, class State, typename CFtype>
void Tester<Input,Output, State,CFtype>::ExecuteRunChoice()
{
  if (sub_choice > 0)
  {    
    Runner<Input,State,CFtype>& r = *runners[sub_choice-1];
    r.ReadParameters();
    
    // Ask for timeout
    double timeout;
    os << "  Timeout: ";
    std::cin >> timeout;
    os << std::endl;
    auto to = std::chrono::milliseconds((long long)(timeout * 1000));

    auto start = std::chrono::high_resolution_clock::now();
    CFtype result = r.SyncRun(to, this->test_state);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = end - start;
    om.OutputState(test_state,out);
    
    os << "CURRENT SOLUTION " << std::endl << out << std::endl;
    os << "CURRENT COST : " << result << std::endl;
    os << "ELAPSED TIME : " << std::chrono::duration_cast<std::chrono::milliseconds>(duration).count() << " ms" << std::endl;
    os << "NUMBER OF ITERATIONS : " << r.Iteration() << std::endl;
  }
}

/**
 Manages an adbridged menu for building the initial state.
 */
template <class Input, class Output, class State, typename CFtype>
void Tester<Input,Output,State,CFtype>::RunInputMenu()
{
  bool show_state;
  ShowReducedStateMenu();
  std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
  show_state = ExecuteStateChoice();
  secs duration = std::chrono::duration_cast<secs>(std::chrono::high_resolution_clock::now() - start);
  if (show_state)
  {
    this->om.OutputState(test_state, this->out);
    os << "INITIAL SOLUTION " << std::endl << this->out << std::endl;
    os << "INITIAL COST : " << this->sm.CostFunction(test_state) << std::endl;
  }
  os << "ELAPSED TIME : " << duration.count() << "s" << std::endl;
}

/**
 Outputs the menu options.
 */
template <class Input, class Output, class State, typename CFtype>
void Tester<Input,Output,State,CFtype>::ShowStateMenu()
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
  std::cin >> sub_choice;
}

/**
 Outputs a reduced set of options for the initial state building.
 */
template <class Input, class Output, class State, typename CFtype>
void Tester<Input,Output,State,CFtype>::ShowReducedStateMenu()
{
  os << "INITIAL STATE MENU: " << std::endl
  << "    (1) Random state " << std::endl
  << "    (2) Read from file" << std::endl
  << "    (3) Greedy state " << std::endl
  << "Your choice : ";
  std::cin >> sub_choice;
  if (sub_choice >= 4)
    sub_choice = -1;
}

/**
 Execute the menu choice on the given state.
 
 @param st the current state
 */
template <class Input, class Output, class State, typename CFtype>
bool Tester<Input,Output,State,CFtype>::ExecuteStateChoice()
{
  unsigned int i;
  std::string file_name;
  switch(sub_choice)
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
      }
      while (read_failed);
      this->om.ReadState(test_state, is);
      break;
    }
    case 3:
    {
      unsigned int lenght;
      double randomness;
      os << "Lenght of the restricted candidate list: ";
      std::cin >> lenght;
      os << "Level of randomness (0 <= alpha <= 1): ";
      std::cin >> randomness;
      this->sm.GreedyState(test_state, randomness, lenght);
      break;
    }
    case 4:
    {
      unsigned int samples;
      os << "How many samples : ";
      std::cin >> samples;
      this->sm.SampleState(test_state,samples);
      break;
    }
    case 5:
    {
      os << "File name : ";
      std::cin >> file_name;
      std::ofstream os(file_name.c_str());
      this->om.WriteState(test_state, os);
      break;
    }
    case 6:
    {
      os  << test_state << std::endl;
      os  << "Total cost: " << this->sm.CostFunction(test_state) << std::endl;
      break;
    }
    case 7:
    {
      os << in;
      break;
    }
    case 8:
    {
      os  << "Cost Components: " << std::endl;
      for (i = 0; i < this->sm.CostComponents(); i++)
      {
        CostComponent<Input,State,CFtype>& cc = this->sm.GetCostComponent(i);
        os  << i << ". " << cc.name << " : "
        << cc.Cost(test_state) << (cc.IsHard() ? '*' : ' ') << std::endl;
      }
      os << "Total Violations: " << this->sm.Violations(test_state) << std::endl;
      os << "Total Objective:  " << this->sm.Objective(test_state) << std::endl;
      os << "Total Cost:       " << this->sm.CostFunction(test_state) << std::endl;
      break;
    }
    case 9:
    {
      os << "Detailed Violations: " << std::endl;
      for (i = 0; i < this->sm.CostComponents(); i++)
      {
        CostComponent<Input,State,CFtype>& cc = this->sm.GetCostComponent(i);
        cc.PrintViolations(test_state);
      }
      os << std::endl << "Summary of Cost Components: " << std::endl;
      for (i = 0; i < this->sm.CostComponents(); i++)
      {
        CostComponent<Input,State,CFtype>& cc = this->sm.GetCostComponent(i);
        os  << i << ". " << cc.name << " : "
        << cc.Cost(test_state) << (cc.IsHard() ? '*' : ' ') << std::endl;
      }
      os << std::endl << "Total Violations:\t" << this->sm.Violations(test_state) << std::endl;
      os << "Total Objective:\t" << this->sm.Objective(test_state) << std::endl;
      os << "Total Cost:  \t" << this->sm.CostFunction(test_state) << std::endl;
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
      this->om.PrettyPrintOutput(test_state,file_name);
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
template <class Input, class Output, class State, typename CFtype>
void Tester<Input,Output,State,CFtype>::RunStateTestMenu()
{
  bool show_state;
  do
  {
    ShowStateMenu();
    if (sub_choice != 0)
    {
      std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
      show_state = ExecuteStateChoice();
      secs duration = std::chrono::duration_cast<secs>(std::chrono::high_resolution_clock::now() - start);
      if (show_state)
      {
        om.OutputState(test_state,out);
        os << "CURRENT SOLUTION " << std::endl << out << std::endl;
        os << "CURRENT COST : " << sm.CostFunction(test_state) << std::endl;
      }
      os << "ELAPSED TIME : " << duration.count() << "s" << std::endl;
    }
  }
  while (sub_choice != 0);
  os << "Leaving state menu" << std::endl;
}

#endif // _TESTER_HH_
