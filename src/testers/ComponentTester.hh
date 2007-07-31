#ifndef COMPONENTTESTER_HH_
#define COMPONENTTESTER_HH_

#include "../basics/EasyLocalObject.hh"
#include "../helpers/StateManager.hh"
#include "../helpers/OutputManager.hh"
#include "../utils/Chronometer.hh"

/** The Abstract Move Tester is an interface for a tester that handles
    moves.
    @ingroup Testers
*/  
template <class Input, class Output, class State, typename CFtype = int>
class ComponentTester
            : public EasyLocalObject
{
public:
    /** The method executes the interactions with the test menu on a given state.
    @param st the state */
    virtual void RunTestMenu(State& st);
    /** The method shall print the menu on a given state. */
    virtual void ShowMenu() = 0;
    /** The method shall execute the choice given by the variable choice,
    @return true if state has been changed
    @param st the state */    
    virtual bool ExecuteChoice(State& st) = 0;
protected:
    ComponentTester( 
    	const Input& in, StateManager<Input,State,CFtype>& e_sm, 
    	OutputManager<Input,Output,State,CFtype>& e_om,
        std::string name);
    /** Virtual destructor. */
    virtual ~ComponentTester() {}
    const Input& in;
    StateManager<Input,State,CFtype>& sm; /**< A pointer to the attached
        state manager. */
    OutputManager<Input,Output,State,CFtype>& om; /**< A pointer to the attached
                output manager. */
    Output out;   /**< The output object. */
    unsigned int choice;   /**< The option currently chosen from the menu. */
    Chronometer chrono; /**< a chronometer */
};

/*************************************************************************
 * Implementation
 *************************************************************************/

/**
   Constructs an abstract tester for components and assign it a name passed 
   as parameter.
   @param nm the name of the tester
   @param sm a pointer to a state manager
   @param om a pointer to an output manager
   @param in a pointer to an input object (NULL for default(
*/
template <class Input, class Output, class State, typename CFtype>
ComponentTester<Input,Output,State,CFtype>::ComponentTester(const Input& i,
        StateManager<Input,State,CFtype>& e_sm,
	OutputManager<Input,Output,State,CFtype>& e_om, std::string name)
  : EasyLocalObject(name), in(i), sm(e_sm), om(e_om), out(in)
{ }

/**
   Manages the component tester menu for the given state.     
   @param st the state to test
*/
template <class Input, class Output, class State, typename CFtype>
void ComponentTester<Input,Output,State,CFtype>::RunTestMenu(State& st)
{
    bool show_state;
    do
    {
        ShowMenu();
        if (choice != 0)
        {
            this->chrono.Reset();
            this->chrono.Start();
            show_state = ExecuteChoice(st);
            this->chrono.Stop();
            if (show_state)
            {
                om.OutputState(st,out);
                std::cout << "CURRENT SOLUTION " << std::endl << out << std::endl;
                std::cout << "CURRENT COST : " << sm.CostFunction(st) << std::endl;
            }
            std::cout << "ELAPSED TIME : " << this->chrono.TotalTime() << 's' << std::endl;
        }
    }
    while (choice != 0);
    std::cout << "Leaving " << GetName() << " menu" << std::endl;
}
 
#endif /*COMPONENTTESTER_HH_*/
