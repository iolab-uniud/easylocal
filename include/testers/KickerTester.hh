#if !defined(_KICKER_TESTER_HH_)
#define _KICKER_TESTER_HH_

#include <chrono>

#include "testers/ComponentTester.hh"
#include "kickers/Kicker.hh"

typedef std::chrono::duration<double, std::ratio<1>> secs;

/** The Kicker Tester allows to test a Kicker.
    @ingroup Testers
*/
template <class Input, class Output, class State, typename CFtype>
class KickerTester
            : public ComponentTester<Input,Output,State,CFtype>
{
public:
    KickerTester(const Input& in,
                 StateManager<Input,State,CFtype>& e_sm,
                 OutputManager<Input,Output,State,CFtype>& e_om,
                 Kicker<Input,State,CFtype>& k,
		 std::string name, std::ostream& o = std::cout);
  KickerTester(const Input& in,
               StateManager<Input,State,CFtype>& e_sm,
               OutputManager<Input,Output,State,CFtype>& e_om,
               Kicker<Input,State,CFtype>& k,
               std::string name, Tester<Input,Output,State,CFtype>& t, std::ostream& o = std::cout);

  void RunMainMenu(State& st);
  unsigned int Modality() const;
protected:
  void PrintKicks(State& st, bool only_improving);
  void PrintKick(State& st);
  void ShowMenu();
  bool ExecuteChoice(State& st);
  const Input& in;
  Output out;   /**< The output object. */
  StateManager<Input,State,CFtype>& sm; /**< A pointer to the attached
        state manager. */
  OutputManager<Input,Output,State,CFtype>& om; /**< A pointer to the attached
                output manager. */
  unsigned int choice;   /**< The option currently chosen from the menu. */
  Kicker<Input,State,CFtype>& kicker;
  std::ostream& os;
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
   Constructs a state tester by providing it links to
   a state manager, an output manager, and an input object.
   
   @param sm a pointer to a compatible state manager
     @param om a pointer to a compatible output manager
     @param in a pointer to an input object
*/
template <class Input, class Output, class State, typename CFtype>
KickerTester<Input,Output,State,CFtype>::KickerTester(
        const Input& i,
        StateManager<Input,State,CFtype>& e_sm,
        OutputManager<Input,Output,State,CFtype>& e_om,
        Kicker<Input,State,CFtype>& k, std::string name, std::ostream& o)
  : ComponentTester<Input,Output,State,CFtype>(name), in(i), out(i), sm(e_sm), om(e_om), kicker(k), os(o)
{ }

template <class Input, class Output, class State, typename CFtype>
KickerTester<Input,Output,State,CFtype>::KickerTester(
                                                      const Input& i,
                                                      StateManager<Input,State,CFtype>& e_sm,
                                                      OutputManager<Input,Output,State,CFtype>& e_om,                                                      
                                                      Kicker<Input,State,CFtype>& k, std::string name, Tester<Input,Output,State,CFtype>& t, std::ostream& o)
: ComponentTester<Input,Output,State,CFtype>(name), in(i), out(i), sm(e_sm), om(e_om), kicker(k), os(o)
{ t.AddKickerTester(*this); }

/**
   Manages the component tester menu for the given state.     
   @param st the state to test
*/
template <class Input, class Output, class State, typename CFtype>
void KickerTester<Input,Output,State,CFtype>::RunMainMenu(State& st)
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
template <class Input, class Output, class State, typename CFtype>
void KickerTester<Input,Output,State,CFtype>::ShowMenu()
{
  if (kicker.SingleKicker())
    {
      os << "Kicker \"" << this->name << "\" Menu (step = " << kicker.Step() << "):" << std::endl
		<< "    (1) Perform Random Kick" << std::endl
		<< "    (2) Perform Best Kick" << std::endl
		<< "    (3) Perform First Improving Kick" << std::endl
		<< "    (-) --- only for Bimodal Kickers --- " << std::endl
		<< "    (-) --- only for Bimodal Kickers --- " << std::endl
		<< "    (6) Show All Kicks" << std::endl
		<< "    (7) Show Current Best Kicks" << std::endl
		<< "    (8) Set Kicker Parameters" << std::endl
		<< "    (0) Return to Main Menu" << std::endl
		<< "Your choice : ";
    }
  else
    {
      os << "Kicker \"" << this->name << "\" Menu (step = " << kicker.Step() << ", pattern = <";
      kicker.PrintPattern(os);
      os << ">:" << std::endl
		<< "    (1) Perform Random Kick" << std::endl
		<< "    (2) Perform Best Kick" << std::endl
		<< "    (3) Perform First Improving Kick" << std::endl
		<< "    (4) Perform Total Best Kick" << std::endl
		<< "    (5) Perform Total First Improving Kick" << std::endl
		<< "    (6) Show All Kicks (for current pattern)" << std::endl
		<< "    (7) Show Current Best Kicks (for current pattern)" << std::endl
		<< "    (8) Set Kicker Parameters" << std::endl
		<< "    (0) Return to Main Menu" << std::endl
		<< "Your choice : ";
    }
      std::cin >> choice;
}

/**
     Execute the menu choice on the given state.
     
     @param st the current state
  */
template <class Input, class Output, class State, typename CFtype>
bool KickerTester<Input,Output,State,CFtype>::ExecuteChoice(State& st)
{
  bool execute_kick = false;
  switch(choice)
    {
    case 1:
      kicker.RandomKick(st);
      execute_kick = true;
      break;
    case 2:
      kicker.BestKick(st);
      execute_kick = true;
      break;
    case 3:
      kicker.FirstImprovingKick(st);
      execute_kick = true;
      break;
    case 4:
      if (!kicker.SingleKicker())
	{
	  kicker.TotalBestKick(st);
	  execute_kick = true;
	}
      break;
    case 5:
      if (!kicker.SingleKicker())
	{
	  kicker.TotalFirstImprovingKick(st);
	  execute_kick = true;
	}
      break;
    case 6:
      PrintKicks(st,false);
      break;
    case 7:
      PrintKicks(st,true);
      break;
    case 8:
      kicker.ReadParameters();
      break;
    default:
      os << "Invalid choice" << std::endl;
    }
  if (execute_kick)
    kicker.MakeKick(st);
  return  execute_kick;
}

template <class Input, class Output, class State, typename CFtype>
void KickerTester<Input,Output,State,CFtype>::PrintKick(State& st)
{
  for (unsigned int i = 0; i < kicker.Step(); i++)
    {
      os << i << " : ";
      kicker.PrintCurrentMoves(i,os);
      os << ",  ";
    }
  os << "Cost : " << kicker.KickCost() << std::endl;
}


template <class Input, class Output, class State, typename CFtype>
void KickerTester<Input,Output,State,CFtype>::PrintKicks(State& st, bool only_improving)
{
  unsigned int count = 0;
  CFtype best_kick_cost;
  kicker.FirstKick(st);
  best_kick_cost = kicker.KickCost();
  PrintKick(st);

  while (kicker.NextKick())
    {
      if (only_improving)
	{
	  if (LessThan(kicker.KickCost(),best_kick_cost))
	    {
	      best_kick_cost = kicker.KickCost();
	      PrintKick(st);
	      count++;
	    }
	}
      else
	{
	  PrintKick(st);	
	  count++;
	}
    }
  os << "Number of kicks : " << count << std::endl;
}

template <class Input, class Output, class State, typename CFtype>
unsigned int KickerTester<Input,Output,State,CFtype>::Modality() const
{
  return 0;
}

#endif // _KICKER_TESTER_HH_