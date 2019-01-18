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
    template <class Input, class State, class CostStructure, class Kicker>
    class KickerTester
    : public ComponentTester<Input, State, CostStructure>,
    public Core::CommandLineParameters::Parametrized,
    public ChoiceReader
    {
    public:
      KickerTester(StateManager<Input, State, CostStructure> &sm,
                   Kicker&k,
                   std::string name,
                   std::ostream &os);
      
      virtual size_t Modality() const;
      
      virtual void RunMainMenu(const Input& in, State &st);
      void InitializeParameters();
      
    protected:
      void PrintKicks(size_t length, const Input& in, const State &st) const;
      void ShowMenu();
      virtual bool ExecuteChoice(const Input& in, State &st);
      StateManager<Input, State, CostStructure> &sm; /**< A pointer to the attached
                                                            state manager. */
      int choice;                                          /**< The option currently chosen from the menu. */
      Kicker &kicker;
      std::ostream &os;
      Parameter<unsigned int> length;
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    template <class Input, class State, class CostStructure, class Kicker>
    KickerTester<Input, State, CostStructure, Kicker>::KickerTester(StateManager<Input, State, CostStructure> &sm,
                                                                    Kicker &k, std::string name,
                                                                    std::ostream &os)
    : ComponentTester<Input, State, CostStructure>(name), Parametrized(name, "Kicker tester parameters"), sm(sm), kicker(k), os(os)
    {
      InitializeParameters();
    }
    
    template <class Input, class State, class CostStructure, class Kicker>
    void KickerTester<Input, State, CostStructure, Kicker>::InitializeParameters()
    {
      length("kick-length", "Kick length", this->parameters);
      length = 3;
    }
    
    /**
     Manages the component tester menu for the given state.
     @param st the state to test
     */
    template <class Input, class State, class CostStructure, class Kicker>
    void KickerTester<Input, State, CostStructure, Kicker>::RunMainMenu(const Input& in, State &st)
    {
      ReadParameters();
      bool show_state;
      do
      {
        ShowMenu();
        if (choice != 0)
        {
          
          std::chrono::high_resolution_clock::time_point start = std::chrono::high_resolution_clock::now();
          show_state = ExecuteChoice(in, st);
          std::chrono::milliseconds duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start);
          
          if (show_state)
          {
            os << "CURRENT SOLUTION " << std::endl;
            sm.DisplayDetailedState(in, st, os);
            os << "CURRENT COST : " << sm.CostFunctionComponents(in, st) << std::endl;
          }
          os << "ELAPSED TIME : " << duration.count() / 1000.0 << " s" << std::endl;
        }
      } while (choice != 0);
      os << "Leaving " << this->name << " menu" << std::endl;
    }
    
    /**
     Outputs the menu options.
     */
    template <class Input, class State, class CostStructure, class Kicker>
    void KickerTester<Input, State, CostStructure, Kicker>::ShowMenu()
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
    template <class Input, class State, class CostStructure, class Kicker>
    bool KickerTester<Input, State, CostStructure, Kicker>::ExecuteChoice(const Input& in, State &st)
    {
      bool execute_kick = false;
      Kick<State, typename Kicker::Move, CostStructure> kick;
      typename Kicker::CostStructure cost;
      try
      {
        switch (choice)
        {
          case 1:
            std::tie(kick, cost) = kicker.SelectRandom(length, in, st);
            os << kick << " " << cost << std::endl;
            execute_kick = true;
            break;
          case 2:
            std::tie(kick, cost) = kicker.SelectBest(length, in, st);
            os << kick << " " << cost << std::endl;
            execute_kick = true;
            break;
          case 3:
            std::tie(kick, cost) = kicker.SelectFirst(length, in, st);
            os << kick << " " << cost << std::endl;
            execute_kick = true;
            break;
          case 4:
            PrintKicks(length, in, st);
            break;
          default:
            os << "Invalid choice" << std::endl;
        }
        if (execute_kick)
          kicker.MakeKick(in, st, kick);
        return execute_kick;
      }
      catch (EmptyNeighborhood)
      {
        os << "Empty neighborhood." << std::endl;
        return false;
      }
    }
    
    template <class Input, class State, class CostStructure, class Kicker>
    void KickerTester<Input, State, CostStructure, Kicker>::PrintKicks(size_t length, const Input& in, const State &st) const
    {
      for (auto it = kicker.begin(length, in, st); it != kicker.end(length, in, st); ++it)
        os << *it << std::endl;
    }
    
    template <class Input, class State, class CostStructure, class Kicker>
    size_t KickerTester<Input, State, CostStructure, Kicker>::Modality() const
    {
      return kicker.Modality();
    }
  } // namespace Debug
  
} // namespace EasyLocal
