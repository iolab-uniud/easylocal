#if !defined(_GENERALIZED_LOCAL_SEARCH_OBSERVER_HH_)
#define _GENERALIZED_LOCAL_SEARCH_OBSERVER_HH_

#include <chrono>

namespace EasyLocal {

  namespace Core {
    template <class Input, class Output, class State, typename CFtype>
    class GeneralizedLocalSearch;
  }
  
  using namespace Core;

  namespace Debug {
        
    typedef std::chrono::duration<double, std::ratio<1>> secs;

    template <class Input, class Output, class State, typename CFtype>
    class GeneralizedLocalSearchObserver
    {
    public:
      GeneralizedLocalSearchObserver(unsigned int verbosity_level, unsigned int plot_level = 0,  std::ostream& log_os = std::cout, std::ostream& plot_os = std::cout);
            
      void NotifyRestart(GeneralizedLocalSearch<Input, Output, State, CFtype>& s, unsigned int restart);
      void NotifyRound(GeneralizedLocalSearch<Input, Output, State, CFtype>& s);
      void NotifyRunnerStart(GeneralizedLocalSearch<Input, Output, State, CFtype>& s);
      void NotifyRunnerStop(GeneralizedLocalSearch<Input, Output, State, CFtype>& s);
      void NotifyKickerStart(GeneralizedLocalSearch<Input, Output, State, CFtype>& s);
      void NotifyKickStep(GeneralizedLocalSearch<Input, Output, State, CFtype>& s, const CFtype& c);
      void NotifyKickerStop(GeneralizedLocalSearch<Input, Output, State, CFtype>& s);
      void SetNotifyRunner() {  notify_runner = true; } 
    protected:
      bool notify_round, notify_runner, notify_kicker, plot_rounds;
      std::ostream &log, &plot;
    };

    template <class Input, class Output, class State, typename CFtype>
    GeneralizedLocalSearchObserver<Input, Output, State, CFtype>::  GeneralizedLocalSearchObserver(unsigned int verbosity_level, unsigned int plot_level,  std::ostream& log_os, std::ostream& plot_os)  : log(log_os), plot(plot_os)
    {
      if (verbosity_level >= 1)
        notify_round = true;
      else
        notify_round = false;
      if (verbosity_level == 2)  
      {
        notify_runner = true;
        notify_kicker = true;
      }
      else
      {
        notify_runner = false;
        notify_kicker = false;
      }
      plot_rounds = (bool) plot_level;
    }

    template <class Input, class Output, class State, typename CFtype>
    void GeneralizedLocalSearchObserver<Input, Output, State, CFtype>::NotifyRestart(GeneralizedLocalSearch<Input, Output, State, CFtype>& s, unsigned int restart)
    {
      if (notify_round)
      {
        log << "Restart " << restart << " trials " << std::endl;
      }
    }

    template <class Input, class Output, class State, typename CFtype>
    void GeneralizedLocalSearchObserver<Input, Output, State, CFtype>::NotifyRound(GeneralizedLocalSearch<Input, Output, State, CFtype>& s)
    {
      if (notify_round)
      {
        log << "Round " << s.rounds << "/" << s.max_rounds << " finished (idle " << s.idle_rounds << "/" << s.max_idle_rounds << ")" << std::endl;
      }
    }

    template <class Input, class Output, class State, typename CFtype>
    void GeneralizedLocalSearchObserver<Input, Output, State, CFtype>::NotifyKickerStart(GeneralizedLocalSearch<Input, Output, State, CFtype>& s)
    {
      if (notify_kicker)
      {
        log << "Start kicker of solver " << s.name << std::endl;
      }
    }

    template <class Input, class Output, class State, typename CFtype>
    void GeneralizedLocalSearchObserver<Input, Output, State, CFtype>::NotifyKickStep(GeneralizedLocalSearch<Input, Output, State, CFtype>& s, const CFtype& cost)
    {
      if (notify_kicker)
      {
        log << "   Kick move, cost: " <<  cost 
          << ", time " << std::chrono::duration_cast<secs>(s.end - s.begin).count()
            << ", step " << s.p_kicker->Step() << std::endl;
      }
    }

    template <class Input, class Output, class State, typename CFtype>
    void GeneralizedLocalSearchObserver<Input, Output, State, CFtype>::NotifyKickerStop(GeneralizedLocalSearch<Input, Output, State, CFtype>& s)
    {
      if (notify_kicker)
      {
        log << "Stop kicker. Cost : " << s.best_state_cost << std::endl;
      }
    }

    template <class Input, class Output, class State, typename CFtype>
    void GeneralizedLocalSearchObserver<Input, Output, State, CFtype>::NotifyRunnerStart(GeneralizedLocalSearch<Input, Output, State, CFtype>& s)
    {
      if (notify_runner)
      {
        log << "Starting runner " << s.current_runner << " of solver " << s.name << std::endl;
      }
    }

    template <class Input, class Output, class State, typename CFtype>
    void GeneralizedLocalSearchObserver<Input, Output, State, CFtype>::NotifyRunnerStop(GeneralizedLocalSearch<Input, Output, State, CFtype>& s)
    {
      if (notify_runner)
      {
        log << "Runner: " << s.current_runner << ", cost: " << s.runners[s.current_runner]->GetStateCost() 
          << ", distance from starting/best states " << s.sm.StateDistance(s.current_state, s.runners[s.current_runner]->GetState())
            << "/" << s.sm.StateDistance(s.best_state, s.runners[s.current_runner]->GetState())
              << " (" << s.runners[s.current_runner]->GetIterationsPerformed() << " iterations" 
                << ", time " << std::chrono::duration_cast<secs>(s.end - s.begin).count()
                  << "), Rounds " << s.rounds << "/" << s.max_rounds << ", Idle rounds " << s.idle_rounds << "/" << s.max_idle_rounds << std::endl;
      }
      if (plot_rounds)
      {
        plot << s.runners[s.current_runner]->name << ", " << s.runners[s.current_runner]->GetStateCost()
          << ", " << s.current_state_cost << ", " 
            << ", time " << std::chrono::duration_cast<secs>(s.end - s.begin).count()
              << s.sm.StateDistance(s.best_state, s.runners[s.current_runner]->GetState())
                << ", " << s.rounds << ", " << s.idle_rounds << std::endl;
      }
    }    
  }
}

#endif // _GENERALIZED_LOCAL_SEARCH_OBSERVER_HH_
