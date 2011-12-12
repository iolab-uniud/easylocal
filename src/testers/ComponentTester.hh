#if !defined(_COMPONENT_TESTER_HH_)
#define _COMPONENT_TESTER_HH_

#include <helpers/StateManager.hh>
#include <helpers/OutputManager.hh>
#include <chrono>

/** The Abstract Move Tester is an interface for a tester that handles
    moves.
    @ingroup Testers
*/  
template <class Input, class Output, class State, typename CFtype = int>
class ComponentTester
{
public:
    /** The method executes the interactions with the test menu on a given state.
    @param st the state */
    virtual void RunMainMenu(State& st) = 0;
    /** The method shall print the menu on a given state. */
    virtual void ShowMenu() = 0;
    /** The method shall execute the choice given by the variable choice,
    @return true if state has been changed
    @param st the state */    
    virtual bool ExecuteChoice(State& st) = 0;
    const std::string name;
    virtual unsigned int Modality() const = 0;
protected:
  ComponentTester(std::string name);
    /** Virtual destructor. */
    virtual ~ComponentTester() {}
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
ComponentTester<Input,Output,State,CFtype>::ComponentTester(std::string e_name)
  : name(e_name) {}


#endif // _COMPONENT_TESTER_HH_
