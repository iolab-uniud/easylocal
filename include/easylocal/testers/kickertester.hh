#if !defined(_KICKER_TESTER_HH_)
#define _KICKER_TESTER_HH_

#include <chrono>

#include "easylocal/testers/componenttester.hh"
#include "easylocal/utils/parameter.hh"
#include "easylocal/helpers/kicker.hh"

namespace EasyLocal {
  
  namespace Debug {
    
    /** The Kicker Tester allows to test a Kicker.
     @ingroup Testers
     */
    template <class Input, class Output, class State, class Move, typename CFtype = int>
    class KickerTester
    : public ComponentTester<Input, Output, State, CFtype>, public Parametrized, public ChoiceReader<Input, State, CFtype>
    {
    public:
      KickerTester(const Input& in,
                   Core::StateManager<Input, State, CFtype>& e_sm,
                   Core::OutputManager<Input, Output, State, CFtype>& e_om,
                   Core::Kicker<Input, State, Move, CFtype>& k,
                   std::string name, Tester<Input, Output, State, CFtype>& t, std::ostream& o = std::cout);
      virtual size_t Modality() const;
      
      void RunMainMenu(State& st);
      void RegisterParameters();
    protected:
      void PrintKicks(size_t length, const State& st) const;
      void ShowMenu();
      bool ExecuteChoice(State& st);
      const Input& in;
      Output out;   /**< The output object. */
      Core::StateManager<Input, State, CFtype>& sm; /**< A pointer to the attached
                                                     state manager. */
      Core::OutputManager<Input, Output, State, CFtype>& om; /**< A pointer to the attached
                                                              output manager. */
      int choice;   /**< The option currently chosen from the menu. */
      Core::Kicker<Input, State, Move, CFtype>& kicker;
      std::ostream& os;
      Parameter<unsigned int> length;
    };
    
    /*************************************************************************
     * Implementation
     *************************************************************************/
    
    template <class Input, class Output, class State, class Move, typename CFtype>
    KickerTester<Input, Output, State, Move, CFtype>::KickerTester(
                                                                   const Input& i,
                                                                   Core::StateManager<Input, State, CFtype>& e_sm,
                                                                   Core::OutputManager<Input, Output, State, CFtype>& e_om,
                                                                   Core::Kicker<Input, State, Move, CFtype>& k, std::string name, Tester<Input, Output, State, CFtype>& t, std::ostream& o)
    : ComponentTester<Input, Output, State, CFtype>(name), Parametrized(name, "Kicker tester parameters"), in(i), out(i), sm(e_sm), om(e_om), kicker(k), os(o)
    { t.AddKickerTester(*this); }
    
    template <class Input, class Output, class State, class Move, typename CFtype>
    void KickerTester<Input, Output, State, Move, CFtype>::RegisterParameters()
    {
      length("kick-length", "Kick length", this->parameters);
      length = 3;
    }
    
    /**
     Manages the component tester menu for the given state.
     @param st the state to test
     */
    template <class Input, class Output, class State, class Move, typename CFtype>
    void KickerTester<Input, Output, State, Move, CFtype>::RunMainMenu(State& st)
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
            os << "CURRENT SOLUTION " << std::endl << out << std::endl;
            os << "CURRENT COST : " << sm.CostFunctionComponents(st) << std::endl;
          }
          os << "ELAPSED TIME : " << duration.count() / 1000.0 << " s" << std::endl;
        }
      }
      while (choice != 0);
      os << "Leaving " << this->name << " menu" << std::endl;
    }
    
    /**
     Outputs the menu options.
     */
    template <class Input, class Output, class State, class Move, typename CFtype>
    void KickerTester<Input, Output, State, Move, CFtype>::ShowMenu()
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
    template <class Input, class Output, class State, class Move, typename CFtype>
    bool KickerTester<Input, Output, State, Move, CFtype>::ExecuteChoice(State& st)
    {
      bool execute_kick = false;
      Kick<State, Move, CFtype> kick;
      CostStructure<CFtype> cost;
      try {
        switch(choice)
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
    
    template <class Input, class Output, class State, class Move, typename CFtype>
    void KickerTester<Input, Output, State, Move, CFtype>::PrintKicks(size_t length, const State& st) const
    {
      for (auto it = kicker.begin(length, st); it != kicker.end(length, st); ++it)
        os << *it << std::endl;
    }
    
    template <class Input, class Output, class State, class Move, typename CFtype>
    size_t KickerTester<Input, Output, State, Move, CFtype>::Modality() const
    { 
      return kicker.Modality(); 
    }
  }
  
}



#endif // define _KICKER_TESTER_HH_