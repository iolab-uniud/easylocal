#ifndef TESTER_HH_
#define TESTER_HH_

#include "../helpers/StateManager.hh"
#include "../helpers/OutputManager.hh"
#include "../basics/EasyLocalObject.hh"
#include "../runners/Runner.hh"
#include "../solvers/SimpleLocalSearch.hh"
#include "../solvers/TokenRingSolver.hh"
#include "ComponentTester.hh"

/** A Tester collects a set of basic testers (move, state, ...) and
    allows to access them through sub-menus. It represent the external
    user interface provided by the framework.
    @ingroup Testers
*/
template <class Input, class Output, class State, typename CFtype = int>
class Tester
            : public EasyLocalObject
{
public:
    Tester(const Input& in, StateManager<Input,State,CFtype>& e_sm, 
    	OutputManager<Input,Output,State,CFtype>& e_om);
    /** Virtual destructor. */
    virtual ~Tester() {}
    void RunMainMenu(string nome_file = "");
    void Print(std::ostream& os = std::cout) const;
    void SetMoveTester(ComponentTester<Input,Output,State,CFtype>* p_amt, unsigned int i);
    void AddMoveTester(ComponentTester<Input,Output,State,CFtype>* p_amt);
    void AddKickerTester(ComponentTester<Input,Output,State,CFtype>* p_kt);
    void CleanMoveTesters();
    void CleanRunners();
    void SetRunner(Runner<Input,State,CFtype>* p_ru, unsigned int i);
    void AddRunner(Runner<Input,State,CFtype>* p_ru);
    void ProcessBatch(const std::string& filename);
    void CleanSolver();
    void Check() const throw(EasyLocalException);
  void RunInputMenu();
  void RunStateTestMenu();
protected:
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
    StateManager<Input,State,CFtype>& sm;  /**< A pointer to a state manager. */
    OutputManager<Input,Output,State,CFtype>& om; /**< A pointer to an output producer. */
  State test_state; /**< The current state managed by the tester. */
  Output out; /**< The output object. */
  // A bunch of solvers
  SimpleLocalSearch<Input,Output,State,CFtype> ss;
  TokenRingSolver<Input,Output,State,CFtype> trs;
  unsigned int choice; /**< The option currently chosen from the menu. */
  int sub_choice; /** The suboption currently chosen from the menu. */
  Chronometer chrono; /** A chronometer */
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
Tester<Input, Output, State,CFtype>::Tester(const Input& i,
    StateManager<Input,State,CFtype>& e_sm,
                                     OutputManager<Input,Output,State,CFtype>& e_om)
  : move_testers(0), runners(0), 
    in(i), sm(e_sm), om(e_om), 
    test_state(i), out(i), ss(i, e_sm, e_om),
    trs(i, e_sm, e_om)
{}

/**
  Sets the i-th move tester as the passed parameter.

  @param p_amt a pointer to a move tester
  @param i the position
*/
template <class Input, class Output, class State, typename CFtype>
void Tester<Input, Output, State,CFtype>::SetMoveTester(ComponentTester<Input,Output,State,CFtype>* p_amt, unsigned int i)
{ assert(i < move_testers.size()); move_testers[i] = p_amt; }

/**
   Adds a move tester.

   @param p_amt a pointer to a move tester
*/
template <class Input, class Output, class State, typename CFtype>
void Tester<Input, Output, State,CFtype>::AddMoveTester(ComponentTester<Input,Output,State,CFtype>* p_amt)
{ move_testers.push_back(p_amt); }

/**
   Removes all the move tester.
*/
template <class Input, class Output, class State, typename CFtype>
void Tester<Input, Output, State,CFtype>::CleanMoveTesters()
{ move_testers.clear(); }

/**
     Adds a kicker tester.

     @param p_amt a pointer to a move tester
*/
template <class Input, class Output, class State, typename CFtype>
void Tester<Input, Output, State,CFtype>::AddKickerTester(ComponentTester<Input,Output,State,CFtype>* p_kt)
{ kicker_testers.push_back(p_kt); }

/**
   Checks wether the object state is consistent with all the related
   objects.
*/
template <class Input, class Output, class State, typename CFtype>
void Tester<Input,Output,State,CFtype>::Check() const
throw(EasyLocalException)
{}

/**
   Outputs the state of the tester on a given output stream.

   @param os the output stream
*/
template <class Input, class Output, class State, typename CFtype>
void  Tester<Input,Output,State,CFtype>::Print(std::ostream& os) const
{
    os  << "Tester State" << std::endl;
    for (unsigned int i = 0; i < this->runners.size(); i++)
    {
        os  << "Runner " << i << std::endl;
        this->runners[i]->Print(os);
    }
}


/**
   Removes all the runners attached to the tester.
*/
template <class Input, class Output, class State, typename CFtype>
void Tester<Input, Output, State,CFtype>::CleanRunners()
{
    this->runners.clear();
}

/**
    Sets the i-th runner attached to the tester to the one passed 
    as parameter.
    
    @param p_ru a pointer to a compatible runner
    @param i the position of the runner
*/
template <class Input, class Output, class State, typename CFtype>
void Tester<Input, Output, State,CFtype>::SetRunner(Runner<Input,State,CFtype>* p_ru, unsigned int i)
{
    runners[i] = p_ru;
}

/**
   Adds a runner to the tester.
   
   @param p_ru a pointer to a compatible runner
*/
template <class Input, class Output, class State, typename CFtype>
void Tester<Input, Output, State,CFtype>::AddRunner(Runner<Input,State,CFtype>* p_ru)
{
    assert(p_ru != NULL);
    runners.push_back(p_ru);
}

/**
   Processes a given batch file of experiments.

   @param filename the name of the batch file
*/
template <class Input, class Output, class State, typename CFtype>
void Tester<Input, Output, State,CFtype>::ProcessBatch(const std::string& filename)
{
}


/**
   Manages the tester main menu.     
*/  

template <class Input, class Output, class State, typename CFtype>
void Tester<Input, Output, State,CFtype>::RunMainMenu(string file_name)
{
  unsigned int i;
  
  for (i = 0; i < move_testers.size(); i++)
    assert(move_testers[i] != NULL);
  for (i = 0; i < runners.size(); i++)
    assert(runners[i] != NULL);
  
  if (file_name == "")
    RunInputMenu();
  else
    {
      std::ifstream is(file_name.c_str());
      assert (!is.fail());
      om.ReadState(test_state, is);
      om.OutputState(test_state, out);
      std::cout << "SOLUTION IMPORTED " << std::endl << out << std::endl;
      std::cout << "IMPORTED SOLUTION COST : " << sm.CostFunction(test_state) << std::endl;
    }
  
  do
    {
      ShowMainMenu();
      if (this->choice != 0)
	ExecuteMainChoice();
    }
  while (this->choice != 0);
  std::cout << "Bye bye..." << std::endl;
}

/**
   Outputs the main menu options.
*/
template <class Input, class Output, class State, typename CFtype>
void Tester<Input,Output, State,CFtype>::ShowMainMenu()
{
    std::cout << "MAIN MENU:" << std::endl
    << "   (1) Move menu" << std::endl
    << "   (2) Kicker menu" << std::endl
    << "   (3) Run menu" << std::endl
    << "   (4) State menu" << std::endl
    << "   (5) Solver menu" << std::endl
    << "   (6) Process batch file" << std::endl
    << "   (7) Debugging" << std::endl
    << "   (0) Exit" << std::endl
    << " Your choice: ";
    std::cin >> this->choice;
}

/**
   Outputs a debugging menu.
*/
template <class Input, class Output, class State, typename CFtype>
void Tester<Input,Output, State,CFtype>::ShowDebuggingMenu()
{
    std::cout << "DEBUGGING MENU:" << std::endl
    << "   (1) Print tester status" << std::endl
    << "   (2) Check tester status" << std::endl
    << " Your choice: ";
    std::cin >> this->choice;
}

/**
   Execute a choice made in the debugging menu.
*/
template <class Input, class Output, class State, typename CFtype>
void Tester<Input, Output,State,CFtype>::ExecuteDebuggingMenu()
{
    switch(this->choice)
    {
    case 1:
        Print();
        try
        {
            Check();
        }
        catch (EasyLocalException& e)
        {
            std::cout << e.toString() << std::endl;
        }
        break;
    case 2:
        try
        {
            Check();
        }
        catch (EasyLocalException& e)
        {
            std::cout << e.toString() <<  std::endl;
        }
        break;
    default:
        std::cout << "Invalid choice" << std::endl;
    }
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
    case 5:
      ShowSolverMenu();
      ExecuteSolverChoice();
      break;
    case 6:
      {
	std::string file_name;
	std::cout << "Insert Batch File name : ";
	std::cin >> file_name;
	ProcessBatch(file_name);
	break;
      }
    case 7:
      ShowDebuggingMenu();
      ExecuteDebuggingMenu();
    case 0:
      break;
    default:
      std::cout << "Invalid choice" << std::endl;
    }
}

/**
   Outputs the menu for the move testers.
*/
template <class Input, class Output, class State, typename CFtype>
void Tester<Input, Output,State,CFtype>::ShowMovesMenu()
{
    unsigned int i;
    std::cout << "MOVE MENU: " << std::endl;
    for (i = 0; i < move_testers.size(); i++)
        std::cout << "   (" << i+1 << ") " << move_testers[i]->GetName() << std::endl;
    std::cout << "   (0) Return to Main Menu" << std::endl;
    std::cout << " Your choice: ";
    std::cin >> sub_choice;
}


template <class Input, class Output, class State, typename CFtype>
void Tester<Input, Output,State,CFtype>::ShowSolverMenu()
{
    //unsigned int i;
    std::cout << "SOLVER MENU: " << std::endl;
    std::cout << "   (1) Simple solver" << std::endl;
    std::cout << "   (2) Token ring solver" << std::endl;
    std::cout << "   (0) Return to Main Menu" << std::endl;
    std::cout << " Your choice: ";
    std::cin >> sub_choice;
}

/**
   Outputs the menu for the move testers.
*/
template <class Input, class Output, class State, typename CFtype>
void Tester<Input, Output,State,CFtype>::ShowKickersMenu()
{
    unsigned int i;
    std::cout << "MOVE MENU: " << std::endl;
    for (i = 0; i < kicker_testers.size(); i++)
        std::cout << "   (" << i+1 << ") " << kicker_testers[i]->GetName() << std::endl;
    std::cout << "   (0) Return to Main Menu" << std::endl;
    std::cout << " Your choice: ";
    std::cin >> sub_choice;
}

/**
   Outputs the menu for the runners.
*/
template <class Input, class Output, class State, typename CFtype>
void Tester<Input,Output, State,CFtype>::ShowRunMenu()
{
    unsigned int i;
    std::cout << "RUN MENU: " << std::endl;
    for (i = 0; i < runners.size(); i++)
        std::cout << "   (" << i+1 << ") " << runners[i]->GetName() << std::endl;
    std::cout << "   (0) Return to Main Menu" << std::endl;
    std::cout << " Your choice: ";
    std::cin >> sub_choice;
}

/**
   Execute a choice from the move testers menu.
*/
template <class Input, class Output, class State, typename CFtype>
void Tester<Input,Output, State,CFtype>::ExecuteMovesChoice()
{
  if (sub_choice > 0 && sub_choice <= (int)move_testers.size())
        move_testers[sub_choice-1]->RunTestMenu(test_state);
}

template <class Input, class Output, class State, typename CFtype>
void Tester<Input,Output, State,CFtype>::ExecuteSolverChoice()
{
	if (sub_choice > 0 && sub_choice <= 2)
		;
} 

/**
    Execute a choice from the move testers menu.
 */
template <class Input, class Output, class State, typename CFtype>
void Tester<Input,Output, State,CFtype>::ExecuteKickersChoice()
{
    if (sub_choice > 0)
        kicker_testers[sub_choice-1]->RunTestMenu(test_state);
}

/**
   Execute a choice from the runners menu.
*/
template <class Input, class Output, class State, typename CFtype>
void Tester<Input,Output, State,CFtype>::ExecuteRunChoice()
{
    if (sub_choice > 0)
    {
        this->chrono.Reset();
        this->chrono.Start();
        ss.SetRunner(*runners[sub_choice-1]);
        ss.ReadParameters();
        ss.SetState(test_state);
        ss.ReSolve();
        this->chrono.Stop();
        test_state = ss.GetState();
        om.OutputState(test_state,out);
        std::cout << "CURRENT SOLUTION " << std::endl << out << std::endl;
        std::cout << "CURRENT COST : " << ss.GetCost() << std::endl;
        std::cout << "ELAPSED TIME : " << this->chrono.TotalTime() << 's' << std::endl;
        std::cout << "NUMBER OF ITERATIONS : " << runners[sub_choice-1]->GetIterationsPerformed() << std::endl;
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
    this->chrono.Reset();
    this->chrono.Start();
    show_state = ExecuteStateChoice();
    this->chrono.Stop();
    if (show_state)
    {
        this->om.OutputState(test_state, this->out);
        std::cout << "INITIAL SOLUTION " << std::endl << this->out << std::endl;
        std::cout << "INITIAL COST : " << this->sm.CostFunction(test_state) << std::endl;
    }
    std::cout << "ELAPSED TIME : " << this->chrono.TotalTime() << 's' << std::endl;
}

/**
   Outputs the menu options.
*/
template <class Input, class Output, class State, typename CFtype>
void Tester<Input,Output,State,CFtype>::ShowStateMenu()
{
    std::cout << "STATE MENU: " << std::endl
    << "    (1) Random state " << std::endl
    << "    (2) Read from file" << std::endl
    << "    (3) Sample state" << std::endl
    << "    (4) Write to file" << std::endl
    << "    (5) Show state" << std::endl
    << "    (6) Show input" << std::endl
    << "    (7) Show cost function components" << std::endl
    << "    (8) Show cost elements" << std::endl
    << "    (9) Pretty print output" << std::endl
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
    std::cout << "INITIAL STATE MENU: " << std::endl
    << "    (1) Random state " << std::endl
    << "    (2) Read from file" << std::endl
    << "Your choice : ";
    std::cin >> sub_choice;
    if (sub_choice >= 3) 
      sub_choice = -1; 
}

/**
   Execute the menu choice on the given state.
   
   @param st the current state
*/
template <class Input, class Output, class State, typename CFtype>
bool Tester<Input,Output,State,CFtype>::ExecuteStateChoice()
{
    switch(sub_choice)
    {
    case 1:
        this->sm.RandomState(test_state);
        break;
    case 2:
        {
            std::string file_name;
	    bool read_failed;
	    std::ifstream is;
            do
            {
	      read_failed = false;
	      std::cout << "File name : ";
	      std::cin >> file_name;
	      is.open(file_name.c_str());
	      if (is.fail())
		{
		  std::cerr << "File " << file_name << " does not exist!" << std::endl;
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
            unsigned int samples;
            std::cout << "How many samples : ";
            std::cin >> samples;
            this->sm.SampleState(test_state,samples);
            break;
        }
    case 4:
        {
            std::string file_name;
            std::cout << "File name : ";
            std::cin >> file_name;
            std::ofstream os(file_name.c_str());
            this->om.WriteState(test_state, os);
            break;
        }
    case 5:
        {
            this->sm.PrintState(test_state);
            break;
        }
    case 6:
        {
            std::cout << this->in;
            break;
        }
    case 7:
        {
            this->sm.PrintStateCost(test_state);
            //    std::cout << std::endl << "Violations: "
            //          << sm.Violations(st) << std::endl;
            //    std::cout << std::endl << "Objective: "
            //          << sm.Objective(st) << std::endl;
            break;
        }
    case 8:
        {
            this->sm.PrintStateDetailedCost(test_state);
            break;
        }
    case 9:
        {
            std::string string_name = "HTML";
            std::cout << "Print output in directory " << std::endl;
            this->om.OutputState(test_state,this->out);
            this->om.PrettyPrintOutput(this->out,string_name);
            break;
        }
    default:
        std::cout << "Invalid choice" << std::endl;
    }
    return (sub_choice >= 1 && sub_choice <= 3);
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
            this->chrono.Reset();
            this->chrono.Start();
            show_state = ExecuteStateChoice();
            this->chrono.Stop();
            if (show_state)
            {
                om.OutputState(test_state,out);
                std::cout << "CURRENT SOLUTION " << std::endl << out << std::endl;
                std::cout << "CURRENT COST : " << sm.CostFunction(test_state) << std::endl;
            }
            std::cout << "ELAPSED TIME : " << this->chrono.TotalTime() << 's' << std::endl;
        }
    }
    while (sub_choice != 0);
    std::cout << "Leaving state menu" << std::endl;
}

#endif /*TESTER_HH_*/
