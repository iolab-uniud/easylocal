#pragma once

namespace EasyLocal
{
  
  namespace Debug
  {
    
    /** The Abstract Move Tester is an interface for a tester that handles
     moves.
     @ingroup Testers
     */
    template <class Input, class Output, class State, class CostStructure = Core::DefaultCostStructure<int>>
    class ComponentTester
    {
    public:
      /** The method executes the interactions with the test menu on a given state.
       @param st the state */
      virtual void RunMainMenu(const Input& in, State &st) = 0;
      /** The method shall print the menu on a given state. */
      virtual void ShowMenu() = 0;
      /** The method shall execute the choice given by the variable choice,
       @return true if state has been changed
       @param st the state */
      virtual bool ExecuteChoice(const Input& in, State &st) = 0;
      const std::string name;
      virtual size_t Modality() const = 0;
      /** Virtual destructor. */
      virtual ~ComponentTester() {}
    protected:
      ComponentTester(std::string name);      
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    /**
     Constructs an abstract tester for components and assign it a name passed
     as parameter.
     @param name the name of the tester
     */
    template <class Input, class Output, class State, class CostStructure>
    ComponentTester<Input, Output, State, CostStructure>::ComponentTester(std::string name)
    : name(name) {}
  } // namespace Debug
} // namespace EasyLocal
