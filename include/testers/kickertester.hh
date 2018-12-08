#pragma once

#include <chrono>

#include "testers/componenttester.hh"
#include "utils/parameter.hh"
#include "helpers/kicker.hh"

namespace EasyLocal
{
  
  namespace Debug
  {
    
    /** The Kicker Tester allows to test a Kicker.
     @ingroup Testers
     */
    template <class Input, class Output, class State, class Move, class CostStructure = DefaultCostStructure<int>>
    class KickerTester
    : public ComponentTester<Input, Output, State, CostStructure>,
    public Core::CommandLineParameters::Parametrized,
    public ChoiceReader
    {
    public:
      typedef typename CostStructure::CFtype CFtype;
      
      KickerTester(const Input &in,
                   Core::StateManager<Input, State, CostStructure> &e_sm,
                   Core::OutputManager<Input, Output, State> &e_om,
                   Core::Kicker<Input, State, Move, CostStructure> &k,
                   std::string name, Tester<Input, Output, State, CostStructure> &t, std::ostream &o = std::cout);
      virtual size_t Modality() const;
      
      void RunMainMenu(State &st);
      void InitializeParameters();
      
    protected:
      void PrintKicks(size_t length, const State &st) const;
      void ShowMenu();
      bool ExecuteChoice(State &st);
      const Input &in;
      Output out;                                          /**< The output object. */
      Core::StateManager<Input, State, CostStructure> &sm; /**< A pointer to the attached
                                                            state manager. */
      Core::OutputManager<Input, Output, State> &om;       /**< A pointer to the attached
                                                            output manager. */
      int choice;                                          /**< The option currently chosen from the menu. */
      Core::Kicker<Input, State, Move, CostStructure> &kicker;
      std::ostream &os;
      Parameter<unsigned int> length;
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    template <class Input, class Output, class State, class Move, class CostStructure>
    KickerTester<Input, Output, State, Move, CostStructure>::KickerTester(
                                                                          const Input &i,
                                                                          Core::StateManager<Input, State, CostStructure> &e_sm,
                                                                          Core::OutputManager<Input, Output, State> &e_om,
                                                                          Core::Kicker<Input, State, Move, CostStructure> &k, std::string name, Tester<Input, Output, State, CostStructure> &t, std::ostream &o)
    : ComponentTester<Input, Output, State, CostStructure>(name), Core::CommandLineParameters::Parametrized(name, "Kicker tester parameters"), in(i), out(i), sm(e_sm), om(e_om), kicker(k), os(o)
    {
      t.AddKickerTester(*this);
    }
    
    template <class Input, class Output, class State, class Move, class CostStructure>
    void KickerTester<Input, Output, State, Move, CostStructure>::InitializeParameters()
    {
      length("kick-length", "Kick length", this->parameters);
      length = 3;
    }
    
    /**
     Manages the component tester menu for the given state.
     @param st the state to test
     */
    template <class Input, class Output, class State, class Move, class CostStructure>
    void KickerTester<Input, Output, State, Move, CostStructure>::RunMainMenu(State &st)
    {
      ReadParameters();
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
            os << "CURRENT SOLUTION " << std::endl
            << out << std::endl;
            os << "CURRENT COST : " << sm.CostFunctionComponents(st) << std::endl;
          }
          os << "ELAPSED TIME : " << duration.count() / 1000.0 << " s" << std::endl;
        }
      } while (choice != 0);
      os << "Leaving " << this->name << " menu" << std::endl;
    }
    
    /**
     Outputs the menu options.
     */
    template <class Input, class Output, class State, class Move, class CostStructure>
    void KickerTester<Input, Output, State, Move, CostStructure>::ShowMenu()
    {
      os << "Kicker \"" << this->name << "\" Menu:" << std::endl
      << "    (1) Perform Random Kick" << std::endl
      << "    (2) Perform Best Kick" << std::endl
      << "    (3) Perform First Improving Kick" << std::endl
      << "    (4) Show All Kicks" << std::endl
      << "    (0) Return to Main Menu" << std::endl
      << "Your choice : ";
      choice = this->ReadChoice(std::cin);
    }
    
    /**
     Execute the menu choice on the given state.
     
     @param st the current state
     */
    template <class Input, class Output, class State, class Move, class CostStructure>
    bool KickerTester<Input, Output, State, Move, CostStructure>::ExecuteChoice(State &st)
    {
      bool execute_kick = false;
      Kick<State, Move, CostStructure> kick;
      DefaultCostStructure<CFtype> cost;
      try
      {
        switch (choice)
        {
          case 1:
            std::tie(kick, cost) = kicker.SelectRandom(length, st);
            os << kick << " " << cost << std::endl;
            execute_kick = true;
            break;
          case 2:
            std::tie(kick, cost) = kicker.SelectBest(length, st);
            os << kick << " " << cost << std::endl;
            execute_kick = true;
            break;
          case 3:
            std::tie(kick, cost) = kicker.SelectFirst(length, st);
            os << kick << " " << cost << std::endl;
            execute_kick = true;
            break;
          case 4:
            PrintKicks(length, st);
            break;
          default:
            os << "Invalid choice" << std::endl;
        }
        if (execute_kick)
          kicker.MakeKick(st, kick);
        return execute_kick;
      }
      catch (EmptyNeighborhood)
      {
        os << "Empty neighborhood." << std::endl;
        return false;
      }
    }
    
    template <class Input, class Output, class State, class Move, class CostStructure>
    void KickerTester<Input, Output, State, Move, CostStructure>::PrintKicks(size_t length, const State &st) const
    {
      for (auto it = kicker.begin(length, st); it != kicker.end(length, st); ++it)
        os << *it << std::endl;
    }
    
    template <class Input, class Output, class State, class Move, class CostStructure>
    size_t KickerTester<Input, Output, State, Move, CostStructure>::Modality() const
    {
      return kicker.Modality();
    }
  } // namespace Debug
  
} // namespace EasyLocal
