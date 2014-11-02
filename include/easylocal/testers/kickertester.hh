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
                   std::string name, std::ostream& o = std::cout);
      
      KickerTester(const Input& in,
                   Core::StateManager<Input, State, CFtype>& e_sm,
                   Core::OutputManager<Input, Output, State, CFtype>& e_om,
                   Core::Kicker<Input, State, Move, CFtype>& k,
                   std::string name, Tester<Input, Output, State, CFtype>& t, std::ostream& o = std::cout);
      virtual unsigned int Modality() const;
      
      void RunMainMenu(State& st);
    protected:
      void PrintKicks(const State& st, unsigned int length, bool only_improving = true) const;
      void PrintKick(const State& st, const std::vector<Move>& kick) const;
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
    
    /**
     Constructs a state tester by providing it links to
     a state manager, an output manager, and an input object.
     
     @param sm a pointer to a compatible state manager
     @param om a pointer to a compatible output manager
     @param in a pointer to an input object
     */
    template <class Input, class Output, class State, class Move, typename CFtype>
    KickerTester<Input, Output, State, Move, CFtype>::KickerTester(
                                                                   const Input& i,
                                                                   Core::StateManager<Input, State, CFtype>& e_sm,
                                                                   Core::OutputManager<Input, Output, State, CFtype>& e_om,
                                                                   Core::Kicker<Input, State, Move, CFtype>& k, std::string name, std::ostream& o)
    :  ComponentTester<Input, Output, State, CFtype>(name), Parametrized(name, "Kicker tester parameters"), in(i), out(i), sm(e_sm), om(e_om), kicker(k), os(o), length("kick-length", "Kick length", this->parameters)
    { length = 3; }
    
    template <class Input, class Output, class State, class Move, typename CFtype>
    KickerTester<Input, Output, State, Move, CFtype>::KickerTester(
                                                                   const Input& i,
                                                                   Core::StateManager<Input, State, CFtype>& e_sm,
                                                                   Core::OutputManager<Input, Output, State, CFtype>& e_om,
                                                                   Core::Kicker<Input, State, Move, CFtype>& k, std::string name, Tester<Input, Output, State, CFtype>& t, std::ostream& o)
    : ComponentTester<Input, Output, State, CFtype>(name), Parametrized(name, "Kicker tester parameters"), in(i), out(i), sm(e_sm), om(e_om), kicker(k), os(o), length("kick-length", "Kick length", this->parameters)
    { t.AddKickerTester(*this); length = 3; }
    
    /**
     Manages the component tester menu for the given state.
     @param st the state to test
     */
    template <class Input, class Output, class State, class Move, typename CFtype>
    void KickerTester<Input, Output, State, Move, CFtype>::RunMainMenu(State& st)
    {
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
            os << "CURRENT COST : " << sm.CostFunction(st) << std::endl;
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
      << "    (5) Show All Improving Kicks" << std::endl
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
      ReadParameters();
      
      bool execute_kick = false;
      std::vector<Move> kick;
      try {
        switch(choice)
        {
          case 1:
            kicker.RandomKick(st, kick, length);
            execute_kick = true;
            break;
          case 2:
            kicker.BestKick(st, kick, length);
            execute_kick = true;
            break;
          case 3:
            kicker.FirstImprovingKick(st, kick, length);
            execute_kick = true;
            break;
          case 4:
            PrintKicks(st, length, false);
            break;
          case 5:
            PrintKicks(st, length);
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
    void KickerTester<Input, Output, State, Move, CFtype>::PrintKick(const State& st, const std::vector<Move>& kick) const
    {
      for (const Move& mv : kick)
      {
        os << mv << " ";
      }
      os << "Cost : " << kicker.DeltaCostFunction(st, kick) << std::endl;
    }
    
    
    template <class Input, class Output, class State, class Move, typename CFtype>
    void KickerTester<Input, Output, State, Move, CFtype>::PrintKicks(const State& st, unsigned int length, bool only_improving) const
    {
      std::vector<Move> kick;
      unsigned count = 0;
      CFtype best_delta_kick_cost;
      kicker.FirstKick(st, kick, length);
      best_delta_kick_cost = kicker.DeltaCostFunction(st, kick);
      PrintKick(st, kick);
      
      while (kicker.NextKick(st, kick))
      {
        if (only_improving)
        {
          CFtype current_delta_kick_cost = kicker.DeltaCostFunction(st, kick);
          if (LessThan(current_delta_kick_cost, best_delta_kick_cost))
          {
            best_delta_kick_cost = current_delta_kick_cost;
            os << count << ": ";
            PrintKick(st, kick);
            count++;
          }
        }
        else
        {
          os << count << ": ";
          PrintKick(st, kick);	
          count++;
        }
      }
      os << "Number of kicks : " << count << std::endl;
    }
    
    template <class Input, class Output, class State, class Move, typename CFtype>
    unsigned int KickerTester<Input, Output, State, Move, CFtype>::Modality() const
    { 
      return kicker.Modality(); 
    }
  }
  
}



#endif // define _KICKER_TESTER_HH_