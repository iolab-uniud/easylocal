#ifndef KICKERTESTER_HH_
#define KICKERTESTER_HH_

#include "ComponentTester.hh"
#include "../kickers/Kicker.hh"

/** The Kicker Tester allows to test a Kicker.
    @ingroup Testers
*/
template <class Input, class Output, class State, typename CFtype = int>
class KickerTester
            : public ComponentTester<Input,Output,State,CFtype>
{
public:
    KickerTester(const Input& in,
                 StateManager<Input,State,CFtype>& e_sm,
                 OutputManager<Input,Output,State,CFtype>& e_om,
                 Kicker<Input,State,CFtype>& k,
		 std::string name
		 );
protected:
    void ShowMenu();
    bool ExecuteChoice(State& st);
    Kicker<Input,State,CFtype>& kicker;
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
        const Input& in,
        StateManager<Input,State,CFtype>& e_sm,
        OutputManager<Input,Output,State,CFtype>& e_om,
        Kicker<Input,State,CFtype>& k, std::string name)
  : ComponentTester<Input,Output,State,CFtype>(in, e_sm, e_om, name), kicker(k)
{ }

/**
    Outputs the menu options.
 */
template <class Input, class Output, class State, typename CFtype>
void KickerTester<Input,Output,State,CFtype>::ShowMenu()
{
  std::cout << "Kicker \"" << this->name << "\" Menu (max step = " << kicker.MaxStep() << "):" << std::endl
	    << "    (1) Random kick" << std::endl
	    << "    (2) Best kick" << std::endl
	    << "    (3) First improving kick" << std::endl
	    << "    (" << (kicker.SingleKicker() ? '-' :  '4') << ") Total Best kick" << std::endl
	    << "    (" << (kicker.SingleKicker() ? '-' :  '5') << ") Total first improving kick" << std::endl
	    << "    (6) Best Dense kick" << std::endl
	    << "    (7) Set Kicker parameters" << std::endl
	    << "    (0) Return to Main Menu" << std::endl
	    << "Your choice : ";
  std::cin >> this->choice;
}

/**
     Execute the menu choice on the given state.
     
     @param st the current state
  */
template <class Input, class Output, class State, typename CFtype>
bool KickerTester<Input,Output,State,CFtype>::ExecuteChoice(State& st)
{
    switch(this->choice)
    {
    case 1:
        kicker.RandomKick(st);
        break;
    case 2:
        kicker.BestKick(st);
        break;
    case 3:
        kicker.FirstImprovingKick(st);
        break;
    case 4:
        kicker.TotalBestKick(st);
        break;
    case 5:
        kicker.TotalFirstImprovingKick(st);
        break;
    case 6:
        kicker.DenseBestKick(st);
        break;
    case 7:
        kicker.ReadParameters();
        break;
    default:
        std::cout << "Invalid choice" << std::endl;
    }
    if (this->choice >= 1 && this->choice <= 6)
      {
	kicker.MakeKick(st);
        return true;
      }
    else
      return false;
} 

#endif /*KICKERTESTER_HH_*/
